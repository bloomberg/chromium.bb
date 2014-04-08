// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_tab_helper.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
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
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

#if defined(CLD2_DYNAMIC_MODE)
#include "base/files/file.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#endif

namespace {

// The maximum number of attempts we'll do to see if the page has finshed
// loading before giving up the translation
const int kMaxTranslateLoadCheckAttempts = 20;

}  // namespace

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
      max_reload_check_attempts_(kMaxTranslateLoadCheckAttempts),
      translate_driver_(&web_contents->GetController()),
      translate_manager_(new TranslateManager(this, prefs::kAcceptLanguages)),
      weak_pointer_factory_(this) {}

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

// static
void TranslateTabHelper::GetTranslateLanguages(
    content::WebContents* web_contents,
    std::string* source,
    std::string* target) {
  DCHECK(source != NULL);
  DCHECK(target != NULL);

  TranslateTabHelper* translate_tab_helper = FromWebContents(web_contents);
  if (!translate_tab_helper)
    return;

  *source = translate_tab_helper->GetLanguageState().original_language();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  PrefService* prefs = original_profile->GetPrefs();
  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    std::string auto_translate_language =
        TranslateManager::GetAutoTargetLanguage(*source, prefs);
    if (!auto_translate_language.empty()) {
      *target = auto_translate_language;
      return;
    }
  }

  std::string accept_languages_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_languages_list;
  base::SplitString(accept_languages_str, ',', &accept_languages_list);
  *target = TranslateManager::GetTargetLanguage(accept_languages_list);
}

TranslateManager* TranslateTabHelper::GetTranslateManager() {
  return translate_manager_.get();
}

content::WebContents* TranslateTabHelper::GetWebContents() {
  return web_contents();
}

void TranslateTabHelper::ShowTranslateUI(translate::TranslateStep step,
                                         const std::string source_language,
                                         const std::string target_language,
                                         TranslateErrors::Type error_type,
                                         bool triggered_from_menu) {
  DCHECK(web_contents());
  if (error_type != TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;

  if (TranslateService::IsTranslateBubbleEnabled()) {
    // Bubble UI.
    if (step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE) {
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
  TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
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

bool TranslateTabHelper::IsTranslatableURL(const GURL& url) {
  return TranslateService::IsTranslatableURL(url);
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

void TranslateTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Check whether this is a reload: When doing a page reload, the
  // TranslateLanguageDetermined IPC is not sent so the translation needs to be
  // explicitly initiated.

  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  // If the navigation happened while offline don't show the translate
  // bar since there will be nothing to translate.
  if (load_details.http_status_code == 0 ||
      load_details.http_status_code == net::HTTP_INTERNAL_SERVER_ERROR) {
    return;
  }

  if (!load_details.is_main_frame &&
      translate_driver_.GetLanguageState().translation_declined()) {
    // Some sites (such as Google map) may trigger sub-frame navigations
    // when the user interacts with the page.  We don't want to show a new
    // infobar if the user already dismissed one in that case.
    return;
  }

  // If not a reload, return.
  if (entry->GetTransitionType() != content::PAGE_TRANSITION_RELOAD &&
      load_details.type != content::NAVIGATION_TYPE_SAME_PAGE) {
    return;
  }

  if (!translate_driver_.GetLanguageState().page_needs_translation())
    return;

  // Note that we delay it as the ordering of the processing of this callback
  // by WebContentsObservers is undefined and might result in the current
  // infobars being removed. Since the translation initiation process might add
  // an infobar, it must be done after that.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TranslateTabHelper::InitiateTranslation,
                 weak_pointer_factory_.GetWeakPtr(),
                 translate_driver_.GetLanguageState().original_language(),
                 0));
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

void TranslateTabHelper::InitiateTranslation(const std::string& page_lang,
                                             int attempt) {
  if (translate_driver_.GetLanguageState().translation_pending())
    return;

  // During a reload we need web content to be available before the
  // translate script is executed. Otherwise we will run the translate script on
  // an empty DOM which will fail. Therefore we wait a bit to see if the page
  // has finished.
  if (web_contents()->IsLoading() && attempt < max_reload_check_attempts_) {
    int backoff = attempt * kMaxTranslateLoadCheckAttempts;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TranslateTabHelper::InitiateTranslation,
                   weak_pointer_factory_.GetWeakPtr(),
                   page_lang,
                   ++attempt),
        base::TimeDelta::FromMilliseconds(backoff));
    return;
  }

  translate_manager_->InitiateTranslation(
      TranslateDownloadManager::GetLanguageCode(page_lang));
}

void TranslateTabHelper::OnLanguageDetermined(
    const LanguageDetectionDetails& details,
    bool page_needs_translation) {
  translate_driver_.GetLanguageState().LanguageDetermined(
      details.adopted_language, page_needs_translation);

  if (web_contents())
    translate_manager_->InitiateTranslation(details.adopted_language);

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
  translate_manager_->PageTranslated(
      original_lang, translated_lang, error_type);

  PageTranslatedDetails details;
  details.source_language = original_lang;
  details.target_language = translated_lang;
  details.error_type = error_type;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<PageTranslatedDetails>(&details));
}

void TranslateTabHelper::ShowBubble(translate::TranslateStep step,
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
  if (step == translate::TRANSLATE_STEP_TRANSLATING ||
      step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) {
    if (GetLanguageState().InTranslateNavigation())
      return;
  }

  TranslateBubbleFactory::Show(
      browser->window(), web_contents(), step, error_type);
#else
  NOTREACHED();
#endif
}
