// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter_observer.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/pdf_unsupported_feature.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker_factory_impl.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sad_tab_observer.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_wrapper_synced_tab_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

static base::LazyInstance<base::PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor = LAZY_INSTANCE_INITIALIZER;

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kAlternateErrorPagesEnabled,
  prefs::kDefaultZoomLevel,
#if defined (ENABLE_SAFE_BROWSING)
  prefs::kSafeBrowsingEnabled,
#endif
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : TabContentsObserver(contents),
      delegate_(NULL),
      in_destructor_(false),
      tab_contents_(contents) {
  DCHECK(contents);
  DCHECK(!GetCurrentWrapperForContents(contents));
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  PrefService* prefs = profile()->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      pref_change_registrar_.Add(kPrefsToObserve[i], this);
  }

  // Create the tab helpers.
  autocomplete_history_manager_.reset(new AutocompleteHistoryManager(contents));
  autofill_manager_ = new AutofillManager(this);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExternalAutofillPopup)) {
    autofill_external_delegate_.reset(
        AutofillExternalDelegate::Create(this, autofill_manager_.get()));
    autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
    autocomplete_history_manager_->SetExternalDelegate(
        autofill_external_delegate_.get());
  }
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
  blocked_content_tab_helper_.reset(new BlockedContentTabHelper(this));
  bookmark_tab_helper_.reset(new BookmarkTabHelper(this));
  constrained_window_tab_helper_.reset(new ConstrainedWindowTabHelper(this));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  infobar_tab_helper_.reset(new InfoBarTabHelper(contents));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  prefs_tab_helper_.reset(new PrefsTabHelper(this));
  prerender_tab_helper_.reset(new prerender::PrerenderTabHelper(this));
  print_view_manager_.reset(new printing::PrintViewManager(this));
  restore_tab_helper_.reset(new RestoreTabHelper(this));
#if defined(ENABLE_SAFE_BROWSING)
  if (profile()->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled) &&
      g_browser_process->safe_browsing_detection_service()) {
    safebrowsing_detection_host_.reset(
        safe_browsing::ClientSideDetectionHost::Create(contents));
  }
#endif
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  ssl_helper_.reset(new TabContentsSSLHelper(this));
  synced_tab_delegate_.reset(new TabContentsWrapperSyncedTabDelegate(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  web_intent_picker_controller_.reset(new WebIntentPickerController(
      this, new WebIntentPickerFactoryImpl()));

  // Create the per-tab observers.
  download_request_limiter_observer_.reset(
      new DownloadRequestLimiterObserver(contents));
  webnavigation_observer_.reset(
      new ExtensionWebNavigationTabObserver(contents));
  external_protocol_observer_.reset(new ExternalProtocolObserver(contents));
  plugin_observer_.reset(new PluginObserver(this));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
  sad_tab_observer_.reset(new SadTabObserver(contents));
  // Start the in-browser thumbnailing if the feature is enabled.
  if (switches::IsInBrowserThumbnailingEnabled()) {
    thumbnail_generation_observer_.reset(new ThumbnailGenerator);
    thumbnail_generation_observer_->StartThumbnailing(tab_contents_.get());
  }

  // Set-up the showing of the omnibox search infobar if applicable.
  if (OmniboxSearchHint::IsEnabled(profile()))
    omnibox_search_hint_.reset(new OmniboxSearchHint(this));

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 content::NotificationService::AllSources());
}

TabContentsWrapper::~TabContentsWrapper() {
  in_destructor_ = true;

  // Need to tear down infobars before the TabContents goes away.
  infobar_tab_helper_.reset();
}

base::PropertyAccessor<TabContentsWrapper*>*
    TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

void TabContentsWrapper::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
}

string16 TabContentsWrapper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

string16 TabContentsWrapper::GetStatusText() const {
  if (!tab_contents()->IsLoading() ||
      tab_contents()->load_state().state == net::LOAD_STATE_IDLE) {
    return string16();
  }

  switch (tab_contents()->load_state().state) {
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
                                        tab_contents()->load_state().param);
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_WAITING_FOR_APPCACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_APPCACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    case net::LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT:
      return l10n_util::GetStringUTF16(
          IDS_LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
    case net::LOAD_STATE_RESOLVING_HOST:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
    case net::LOAD_STATE_CONNECTING:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
    case net::LOAD_STATE_SSL_HANDSHAKE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
    case net::LOAD_STATE_SENDING_REQUEST:
      if (tab_contents()->upload_size())
        return l10n_util::GetStringFUTF16Int(
                    IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
                    static_cast<int>((100 * tab_contents()->upload_position()) /
                        tab_contents()->upload_size()));
      else
        return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                        tab_contents()->load_state_host());
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return string16();
}

TabContentsWrapper* TabContentsWrapper::Clone() {
  TabContents* new_contents = tab_contents()->Clone();
  TabContentsWrapper* new_wrapper = new TabContentsWrapper(new_contents);

  new_wrapper->extension_tab_helper()->CopyStateFrom(
      *extension_tab_helper_.get());
  return new_wrapper;
}

void TabContentsWrapper::CaptureSnapshot() {
  Send(new ChromeViewMsg_CaptureSnapshot(routing_id()));
}

// static
TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    TabContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

// static
const TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    const TabContents* contents) {
  TabContentsWrapper* const* wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

Profile* TabContentsWrapper::profile() const {
  return Profile::FromBrowserContext(tab_contents()->browser_context());
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper implementation:

void TabContentsWrapper::RenderViewCreated(RenderViewHost* render_view_host) {
  UpdateAlternateErrorPageURL(render_view_host);
}

void TabContentsWrapper::DidBecomeSelected() {
  WebCacheManager::GetInstance()->ObserveActivity(
      tab_contents()->GetRenderProcessHost()->GetID());
}

bool TabContentsWrapper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabContentsWrapper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_Snapshot, OnSnapshot)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PDFHasUnsupportedFeature,
                        OnPDFHasUnsupportedFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabContentsWrapper::TabContentsDestroyed(TabContents* tab) {
  // Destruction of the TabContents should only be done by us from our
  // destructor. Otherwise it's very likely we (or one of the helpers we own)
  // will attempt to access the TabContents and we'll crash.
  DCHECK(in_destructor_);
}

void TabContentsWrapper::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_URL_UPDATED:
      UpdateAlternateErrorPageURL(render_view_host());
      break;
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = content::Details<std::string>(details).ptr();
      DCHECK(content::Source<PrefService>(source).ptr() ==
             profile()->GetPrefs());
      if (*pref_name_in == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL(render_view_host());
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
        tab_contents()->render_view_host()->SetZoomLevel(
            tab_contents()->GetZoomLevel());
      } else if (*pref_name_in == prefs::kSafeBrowsingEnabled) {
        UpdateSafebrowsingDetectionHost();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void TabContentsWrapper::OnSnapshot(const SkBitmap& bitmap) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_SNAPSHOT_TAKEN,
      content::Source<TabContentsWrapper>(this),
      content::Details<const SkBitmap>(&bitmap));
}

void TabContentsWrapper::OnPDFHasUnsupportedFeature() {
  PDFHasUnsupportedFeature(this);
}

GURL TabContentsWrapper::GetAlternateErrorPageURL() const {
  GURL url;
  // Disable alternate error pages when in Incognito mode.
  if (profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = profile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(
        GURL(google_util::kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

void TabContentsWrapper::UpdateAlternateErrorPageURL(RenderViewHost* rvh) {
  rvh->SetAltErrorPageURL(GetAlternateErrorPageURL());
}

void TabContentsWrapper::UpdateSafebrowsingDetectionHost() {
#if defined(ENABLE_SAFE_BROWSING)
  PrefService* prefs = profile()->GetPrefs();
  bool safe_browsing = prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  if (safe_browsing &&
      g_browser_process->safe_browsing_detection_service()) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_.reset(
          safe_browsing::ClientSideDetectionHost::Create(tab_contents()));
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
  render_view_host()->Send(
      new ChromeViewMsg_SetClientSidePhishingDetection(routing_id(),
                                                       safe_browsing));
#endif
}

void TabContentsWrapper::ExitFullscreenMode() {
  if (tab_contents() && render_view_host())
    tab_contents()->render_view_host()->ExitFullscreen();
}
