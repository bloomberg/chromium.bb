// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_accept_languages_factory.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

#if defined(CLD2_DYNAMIC_MODE)
#include "base/files/file.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TranslateTabHelper);

#if defined(CLD2_DYNAMIC_MODE)
// Statics defined in the .h file:
base::File* TranslateTabHelper::s_cached_file_ = NULL;
uint64 TranslateTabHelper::s_cached_data_offset_ = 0;
uint64 TranslateTabHelper::s_cached_data_length_ = 0;
base::LazyInstance<base::Lock> TranslateTabHelper::s_file_lock_ =
    LAZY_INSTANCE_INITIALIZER;
#endif

TranslateTabHelper::TranslateTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
#if defined(CLD2_DYNAMIC_MODE)
      weak_pointer_factory_(this),
#endif
      translate_driver_(&web_contents->GetController()),
      translate_manager_(new TranslateManager(this, prefs::kAcceptLanguages)) {}

TranslateTabHelper::~TranslateTabHelper() {
}

LanguageState& TranslateTabHelper::GetLanguageState() {
  return translate_driver_.GetLanguageState();
}

// static
scoped_ptr<TranslatePrefs> TranslateTabHelper::CreateTranslatePrefs(
    PrefService* prefs) {
#if defined(OS_CHROMEOS)
  const char* preferred_languages_prefs = prefs::kLanguagePreferredLanguages;
#else
  const char* preferred_languages_prefs = NULL;
#endif
  return scoped_ptr<TranslatePrefs>(new TranslatePrefs(
      prefs, prefs::kAcceptLanguages, preferred_languages_prefs));
}

// static
TranslateAcceptLanguages* TranslateTabHelper::GetTranslateAcceptLanguages(
    content::BrowserContext* browser_context) {
  return TranslateAcceptLanguagesFactory::GetForBrowserContext(browser_context);
}

// static
TranslateManager* TranslateTabHelper::GetManagerFromWebContents(
    content::WebContents* web_contents) {
  TranslateTabHelper* translate_tab_helper = FromWebContents(web_contents);
  if (!translate_tab_helper)
    return NULL;
  return translate_tab_helper->GetTranslateManager();
}

TranslateManager* TranslateTabHelper::GetTranslateManager() {
  return translate_manager_.get();
}

content::WebContents* TranslateTabHelper::GetWebContents() {
  return web_contents();
}

void TranslateTabHelper::ShowTranslateUI(TranslateTabHelper::TranslateStep step,
                                         const std::string source_language,
                                         const std::string target_language,
                                         TranslateErrors::Type error_type,
                                         bool triggered_from_menu) {
  DCHECK(web_contents());
  if (error_type != TranslateErrors::NONE)
    step = TranslateTabHelper::TRANSLATE_ERROR;

  if (TranslateService::IsTranslateBubbleEnabled()) {
    // Bubble UI.
    if (step == BEFORE_TRANSLATE) {
      // TODO: Move this logic out of UI code.
      GetLanguageState().SetTranslateEnabled(true);
      if (!GetLanguageState().HasLanguageChanged())
        return;
    }
    ShowBubble(step, error_type);
    return;
  }

  // Infobar UI.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  TranslateInfoBarDelegate::Create(step != BEFORE_TRANSLATE,
                                   web_contents(),
                                   step,
                                   source_language,
                                   target_language,
                                   error_type,
                                   original_profile->GetPrefs(),
                                   triggered_from_menu);
}

TranslateDriver* TranslateTabHelper::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* TranslateTabHelper::GetPrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetOriginalProfile()->GetPrefs();
}

scoped_ptr<TranslatePrefs> TranslateTabHelper::GetTranslatePrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return CreateTranslatePrefs(profile->GetPrefs());
}

TranslateAcceptLanguages* TranslateTabHelper::GetTranslateAcceptLanguages() {
  DCHECK(web_contents());
  return GetTranslateAcceptLanguages(web_contents()->GetBrowserContext());
}

bool TranslateTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TranslateTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_TranslateLanguageDetermined,
                        OnLanguageDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PageTranslated, OnPageTranslated)
#if defined(CLD2_DYNAMIC_MODE)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_NeedCLDData, OnCLDDataRequested)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void TranslateTabHelper::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Let the LanguageState clear its state.
  translate_driver_.DidNavigate(details);
}

void TranslateTabHelper::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with NULL WebContents.
  translate_manager_.reset();
}

#if defined(CLD2_DYNAMIC_MODE)
void TranslateTabHelper::OnCLDDataRequested() {
  // Quickly try to read s_cached_file_. If valid, the file handle is
  // cached and can be used immediately. Else, queue the caching task to the
  // blocking pool.
  base::File* handle = NULL;
  uint64 data_offset = 0;
  uint64 data_length = 0;
  {
    base::AutoLock lock(s_file_lock_.Get());
    handle = s_cached_file_;
    data_offset = s_cached_data_offset_;
    data_length = s_cached_data_length_;
  }

  if (handle && handle->IsValid()) {
    // Cached data available. Respond to the request.
    SendCLDDataAvailable(handle, data_offset, data_length);
    return;
  }

  // Else, we don't have the data file yet. Queue a caching attempt.
  // The caching attempt happens in the blocking pool because it may involve
  // arbitrary filesystem access.
  // After the caching attempt is made, we call MaybeSendCLDDataAvailable
  // to pass the file handle to the renderer. This only results in an IPC
  // message if the caching attempt was successful.
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&TranslateTabHelper::HandleCLDDataRequest),
      base::Bind(&TranslateTabHelper::MaybeSendCLDDataAvailable,
                 weak_pointer_factory_.GetWeakPtr()));
}

void TranslateTabHelper::MaybeSendCLDDataAvailable() {
  base::File* handle = NULL;
  uint64 data_offset = 0;
  uint64 data_length = 0;
  {
    base::AutoLock lock(s_file_lock_.Get());
    handle = s_cached_file_;
    data_offset = s_cached_data_offset_;
    data_length = s_cached_data_length_;
  }

  if (handle && handle->IsValid())
    SendCLDDataAvailable(handle, data_offset, data_length);
}

void TranslateTabHelper::SendCLDDataAvailable(const base::File* handle,
                                              const uint64 data_offset,
                                              const uint64 data_length) {
  // Data available, respond to the request.
  IPC::PlatformFileForTransit ipc_platform_file =
      IPC::GetFileHandleForProcess(
          handle->GetPlatformFile(),
          GetWebContents()->GetRenderViewHost()->GetProcess()->GetHandle(),
          false);
  // In general, sending a response from within the code path that is processing
  // a request is discouraged because there is potential for deadlock (if the
  // methods are sent synchronously) or loops (if the response can trigger a
  // new request). Neither of these concerns is relevant in this code, so
  // sending the response from within the code path of the request handler is
  // safe.
  Send(new ChromeViewMsg_CLDDataAvailable(
      GetWebContents()->GetRenderViewHost()->GetRoutingID(),
      ipc_platform_file, data_offset, data_length));
}

void TranslateTabHelper::HandleCLDDataRequest() {
  // Because this function involves arbitrary file system access, it must run
  // on the blocking pool.
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  {
    base::AutoLock lock(s_file_lock_.Get());
    if (s_cached_file_)
      return; // Already done, duplicate request
  }

  base::FilePath path;
  if (!PathService::Get(chrome::DIR_USER_DATA, &path)) {
    LOG(WARNING) << "Unable to locate user data directory";
    return; // Chrome isn't properly installed.
  }

  // If the file exists, we can send an IPC-safe construct back to the
  // renderer process immediately.
  path = path.Append(chrome::kCLDDataFilename);
  if (!base::PathExists(path))
    return;

  // Attempt to open the file for reading.
  scoped_ptr<base::File> file(
      new base::File(path, base::File::FLSG_OPEN | base::File::FLAG_READ);
  if (!file->IsValid()) {
    LOG(WARNING) << "CLD data file exists but cannot be opened";
    return;
  }

  base::File::Info file_info;
  if (!file->GetInfo(&file_info)) {
    LOG(WARNING) << "CLD data file exists but cannot be inspected";
    return;
  }

  // For now, our offset and length are simply 0 and the length of the file,
  // respectively. If we later decide to include the CLD2 data file inside of
  // a larger binary context, these params can be twiddled appropriately.
  const uint64 data_offset = 0;
  const uint64 data_length = file_info.size;

  {
    base::AutoLock lock(s_file_lock_.Get());
    if (s_cached_file_) {
      // Idempotence: Racing another request on the blocking pool, abort.
    } else {
      // Else, this request has taken care of it all. Cache all info.
      s_cached_file_ = file.release();
      s_cached_data_offset_ = data_offset;
      s_cached_data_length_ = data_length;
    }
  }
}
#endif // defined(CLD2_DYNAMIC_MODE)

void TranslateTabHelper::OnLanguageDetermined(
    const LanguageDetectionDetails& details,
    bool page_needs_translation) {
  translate_driver_.GetLanguageState().LanguageDetermined(
      details.adopted_language, page_needs_translation);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<const LanguageDetectionDetails>(&details));
}

void TranslateTabHelper::OnPageTranslated(int32 page_id,
                                          const std::string& original_lang,
                                          const std::string& translated_lang,
                                          TranslateErrors::Type error_type) {
  DCHECK(web_contents());
  translate_driver_.GetLanguageState().SetCurrentLanguage(translated_lang);
  translate_driver_.GetLanguageState().set_translation_pending(false);
  PageTranslatedDetails details;
  details.source_language = original_lang;
  details.target_language = translated_lang;
  details.error_type = error_type;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<PageTranslatedDetails>(&details));
}

void TranslateTabHelper::ShowBubble(TranslateTabHelper::TranslateStep step,
                                    TranslateErrors::Type error_type) {
// The bubble is implemented only on the desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    TranslateBubbleFactory::Show(NULL, web_contents(), step, error_type);
    return;
  }

  if (web_contents() != browser->tab_strip_model()->GetActiveWebContents())
    return;

  // This ShowBubble function is also used for upating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser !=
      chrome::FindLastActiveWithHostDesktopType(browser->host_desktop_type())) {
    return;
  }

  // During auto-translating, the bubble should not be shown.
  if (step == TranslateTabHelper::TRANSLATING ||
      step == TranslateTabHelper::AFTER_TRANSLATE) {
    if (GetLanguageState().InTranslateNavigation())
      return;
  }

  TranslateBubbleFactory::Show(
      browser->window(), web_contents(), step, error_type);
#else
  NOTREACHED();
#endif
}
