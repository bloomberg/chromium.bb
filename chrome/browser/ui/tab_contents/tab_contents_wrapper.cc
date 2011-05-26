// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/pdf_unsupported_feature.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_observer.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/download/download_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webpreferences.h"

namespace {

static base::LazyInstance<PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor(base::LINKER_INITIALIZED);

// The list of prefs we want to observe.
const char* kPrefsToObserve[] = {
  prefs::kAlternateErrorPagesEnabled,
  prefs::kDefaultCharset,
  prefs::kDefaultZoomLevel,
  prefs::kEnableReferrers
};

const int kPrefsToObserveLength = arraysize(kPrefsToObserve);

}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : TabContentsObserver(contents),
      delegate_(NULL),
      infobars_enabled_(true),
      tab_contents_(contents) {
  DCHECK(contents);
  DCHECK(!GetCurrentWrapperForContents(contents));
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  // Create the tab helpers.
  autocomplete_history_manager_.reset(new AutocompleteHistoryManager(contents));
  autofill_manager_.reset(new AutofillManager(this));
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
  blocked_content_tab_helper_.reset(new BlockedContentTabHelper(this));
  bookmark_tab_helper_.reset(new BookmarkTabHelper(this));
  download_tab_helper_.reset(new DownloadTabHelper(this));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  safebrowsing_detection_host_.reset(
      safe_browsing::ClientSideDetectionHost::Create(contents));
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  ssl_helper_.reset(new TabContentsSSLHelper(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  print_view_manager_.reset(new printing::PrintViewManager(contents));

  // Create the per-tab observers.
  external_protocol_observer_.reset(new ExternalProtocolObserver(contents));
  file_select_observer_.reset(new FileSelectObserver(contents));
  plugin_observer_.reset(new PluginObserver(this));
  prerender_observer_.reset(new prerender::PrerenderObserver(contents));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
  webnavigation_observer_.reset(
      new ExtensionWebNavigationTabObserver(contents));
  // Start the in-browser thumbnailing if the feature is enabled.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInBrowserThumbnailing)) {
    thumbnail_generation_observer_.reset(new ThumbnailGenerator);
    thumbnail_generation_observer_->StartThumbnailing(tab_contents_.get());
  }

  // Set-up the showing of the omnibox search infobar if applicable.
  if (OmniboxSearchHint::IsEnabled(contents->profile()))
    omnibox_search_hint_.reset(new OmniboxSearchHint(this));

  registrar_.Add(this, NotificationType::GOOGLE_URL_UPDATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::USER_STYLE_SHEET_UPDATED,
                 NotificationService::AllSources());
#if defined(OS_LINUX)
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
#endif

  // Register for notifications about all interested prefs change.
  PrefService* prefs = profile()->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    for (int i = 0; i < kPrefsToObserveLength; ++i)
      pref_change_registrar_.Add(kPrefsToObserve[i], this);
  }

  renderer_preferences_util::UpdateFromSystemSettings(
      tab_contents()->GetMutableRendererPrefs(), profile());
}

TabContentsWrapper::~TabContentsWrapper() {
  // Notify any lasting InfobarDelegates that have not yet been removed that
  // whatever infobar they were handling in this TabContents has closed,
  // because the TabContents is going away entirely.
  // This must happen after the TAB_CONTENTS_DESTROYED notification as the
  // notification may trigger infobars calls that access their delegate. (and
  // some implementations of InfoBarDelegate do delete themselves on
  // InfoBarClosed()).
  for (size_t i = 0; i < infobar_count(); ++i)
    infobar_delegates_[i]->InfoBarClosed();
  infobar_delegates_.clear();
}

PropertyAccessor<TabContentsWrapper*>* TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

void TabContentsWrapper::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);

  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                             pref_defaults.web_security_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically,
      true,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitLoadsImagesAutomatically,
                             pref_defaults.loads_images_automatically,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitPluginsEnabled,
                             pref_defaults.plugins_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitDomPasteEnabled,
                             pref_defaults.dom_paste_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitShrinksStandaloneImagesToFit,
                             pref_defaults.shrinks_standalone_images_to_fit,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kWebKitInspectorSettings,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitTextAreasAreResizable,
                             pref_defaults.text_areas_are_resizable,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebKitJavaEnabled,
                             pref_defaults.java_enabled,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebkitTabsToLinks,
                             pref_defaults.tabs_to_links,
                             PrefService::UNSYNCABLE_PREF);

#if !defined(OS_MACOSX)
  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES,
                                     PrefService::SYNCABLE_PREF);
#else
  // Not used in OSX.
  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES,
                                     PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterLocalizedStringPref(prefs::kDefaultCharset,
                                     IDS_DEFAULT_ENCODING,
                                     PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitStandardFontFamily,
                                     IDS_STANDARD_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumLogicalFontSize,
                                      IDS_MINIMUM_LOGICAL_FONT_SIZE,
                                      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitUsesUniversalDetector,
                                      IDS_USES_UNIVERSAL_DETECTOR,
                                      PrefService::SYNCABLE_PREF);
  prefs->RegisterLocalizedStringPref(prefs::kStaticEncodings,
                                     IDS_STATIC_ENCODING_LIST,
                                     PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding,
                            "",
                            PrefService::UNSYNCABLE_PREF);
}

string16 TabContentsWrapper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

string16 TabContentsWrapper::GetStatusText() const {
  if (!tab_contents()->is_loading() ||
      tab_contents()->load_state() == net::LOAD_STATE_IDLE) {
    return string16();
  }

  switch (tab_contents()->load_state()) {
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
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
  Send(new ViewMsg_CaptureSnapshot(routing_id()));
}

TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    TabContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper implementation:

void TabContentsWrapper::RenderViewCreated(RenderViewHost* render_view_host) {
  UpdateAlternateErrorPageURL(render_view_host);
}

void TabContentsWrapper::RenderViewGone() {
  // Remove all infobars.
  while (!infobar_delegates_.empty())
    RemoveInfoBar(GetInfoBarDelegateAt(infobar_count() - 1));
}

void TabContentsWrapper::DidBecomeSelected() {
  WebCacheManager::GetInstance()->ObserveActivity(
      tab_contents()->GetRenderProcessHost()->id());
}

bool TabContentsWrapper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabContentsWrapper, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_JSOutOfMemory, OnJSOutOfMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterProtocolHandler,
                        OnRegisterProtocolHandler)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Snapshot, OnSnapshot)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PDFHasUnsupportedFeature,
                        OnPDFHasUnsupportedFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabContentsWrapper::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED: {
      DCHECK(&tab_contents_->controller() ==
             Source<NavigationController>(source).ptr());

      NavigationController::LoadCommittedDetails& committed_details =
          *(Details<NavigationController::LoadCommittedDetails>(details).ptr());

      // NOTE: It is not safe to change the following code to count upwards or
      // use iterators, as the RemoveInfoBar() call synchronously modifies our
      // delegate list.
      for (size_t i = infobar_delegates_.size(); i > 0; --i) {
        InfoBarDelegate* delegate = infobar_delegates_[i - 1];
        if (delegate->ShouldExpire(committed_details))
          RemoveInfoBar(delegate);
      }

      break;
    }
    case NotificationType::GOOGLE_URL_UPDATED:
      UpdateAlternateErrorPageURL(render_view_host());
      break;
    case NotificationType::USER_STYLE_SHEET_UPDATED:
      UpdateWebPreferences();
      break;
#if defined(OS_LINUX)
    case NotificationType::BROWSER_THEME_CHANGED: {
      UpdateRendererPreferences();
      break;
    }
#endif
    case NotificationType::PREF_CHANGED: {
      std::string* pref_name_in = Details<std::string>(details).ptr();
      DCHECK(Source<PrefService>(source).ptr() == profile()->GetPrefs());
      if (*pref_name_in == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL(render_view_host());
      } else if ((*pref_name_in == prefs::kDefaultCharset) ||
                 StartsWithASCII(*pref_name_in, "webkit.webprefs.", true)) {
        UpdateWebPreferences();
      } else if (*pref_name_in == prefs::kDefaultZoomLevel) {
        Send(new ViewMsg_SetZoomLevel(
            routing_id(), tab_contents()->GetZoomLevel()));
      } else if (*pref_name_in == prefs::kEnableReferrers) {
        UpdateRendererPreferences();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void TabContentsWrapper::AddInfoBar(InfoBarDelegate* delegate) {
  if (!infobars_enabled_) {
    delegate->InfoBarClosed();
    return;
  }

  // Look through the existing InfoBarDelegates we have for a match. If we've
  // already got one that matches, then we don't add the new one.
  for (size_t i = 0; i < infobar_delegates_.size(); ++i) {
    if (infobar_delegates_[i]->EqualsDelegate(delegate)) {
      // Tell the new infobar to close so that it can clean itself up.
      delegate->InfoBarClosed();
      return;
    }
  }

  infobar_delegates_.push_back(delegate);
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
      Source<TabContents>(tab_contents_.get()),
      Details<InfoBarDelegate>(delegate));

  // Add ourselves as an observer for navigations the first time a delegate is
  // added. We use this notification to expire InfoBars that need to expire on
  // page transitions.
  if (infobar_delegates_.size() == 1) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(&tab_contents_->controller()));
  }
}

void TabContentsWrapper::RemoveInfoBar(InfoBarDelegate* delegate) {
  if (!infobars_enabled_)
    return;

  std::vector<InfoBarDelegate*>::iterator it =
      find(infobar_delegates_.begin(), infobar_delegates_.end(), delegate);
  if (it != infobar_delegates_.end()) {
    InfoBarDelegate* delegate = *it;
    NotificationService::current()->Notify(
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(tab_contents_.get()),
        Details<InfoBarDelegate>(delegate));

    infobar_delegates_.erase(it);
    // Remove ourselves as an observer if we are tracking no more InfoBars.
    if (infobar_delegates_.empty()) {
      registrar_.Remove(
          this, NotificationType::NAV_ENTRY_COMMITTED,
          Source<NavigationController>(&tab_contents_->controller()));
    }
  }
}

void TabContentsWrapper::ReplaceInfoBar(InfoBarDelegate* old_delegate,
                                        InfoBarDelegate* new_delegate) {
  if (!infobars_enabled_) {
    new_delegate->InfoBarClosed();
    return;
  }

  std::vector<InfoBarDelegate*>::iterator it =
      find(infobar_delegates_.begin(), infobar_delegates_.end(), old_delegate);
  DCHECK(it != infobar_delegates_.end());

  // Notify the container about the change of plans.
  scoped_ptr<std::pair<InfoBarDelegate*, InfoBarDelegate*> > details(
    new std::pair<InfoBarDelegate*, InfoBarDelegate*>(
        old_delegate, new_delegate));
  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_INFOBAR_REPLACED,
      Source<TabContents>(tab_contents_.get()),
      Details<std::pair<InfoBarDelegate*, InfoBarDelegate*> >(details.get()));

  // Remove the old one.
  infobar_delegates_.erase(it);

  // Add the new one.
  DCHECK(find(infobar_delegates_.begin(),
              infobar_delegates_.end(), new_delegate) ==
         infobar_delegates_.end());
  infobar_delegates_.push_back(new_delegate);
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void TabContentsWrapper::OnJSOutOfMemory() {
  AddInfoBar(new SimpleAlertInfoBarDelegate(tab_contents(),
      NULL, l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT), true));
}

void TabContentsWrapper::OnRegisterProtocolHandler(const std::string& protocol,
                                                   const GURL& url,
                                                   const string16& title) {
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  if (policy->IsPseudoScheme(protocol) || policy->IsDisabledScheme(protocol))
    return;

  ProtocolHandlerRegistry* registry = profile()->GetProtocolHandlerRegistry();
  if (!registry->enabled())
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);
  if (!handler.IsEmpty() &&
      registry->CanSchemeBeOverridden(handler.protocol())) {
    AddInfoBar(registry->IsRegistered(handler) ?
        static_cast<InfoBarDelegate*>(new SimpleAlertInfoBarDelegate(
            tab_contents(), NULL, l10n_util::GetStringFUTF16(
                IDS_REGISTER_PROTOCOL_HANDLER_ALREADY_REGISTERED,
                handler.title(), UTF8ToUTF16(handler.protocol())), true)) :
      new RegisterProtocolHandlerInfoBarDelegate(tab_contents(), registry,
                                                 handler));
  }
}

void TabContentsWrapper::OnSnapshot(const SkBitmap& bitmap) {
  NotificationService::current()->Notify(
      NotificationType::TAB_SNAPSHOT_TAKEN,
      Source<TabContentsWrapper>(this),
      Details<const SkBitmap>(&bitmap));
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
  rvh->Send(new ViewMsg_SetAltErrorPageURL(
      rvh->routing_id(), GetAlternateErrorPageURL()));
}

void TabContentsWrapper::UpdateWebPreferences() {
  RenderViewHostDelegate* rvhd = tab_contents();
  Send(new ViewMsg_UpdateWebPreferences(routing_id(), rvhd->GetWebkitPrefs()));
}

void TabContentsWrapper::UpdateRendererPreferences() {
  renderer_preferences_util::UpdateFromSystemSettings(
      tab_contents()->GetMutableRendererPrefs(), profile());
  render_view_host()->SyncRendererPrefs();
}
