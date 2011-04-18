// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/lazy_instance.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_observer.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/download/download_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webpreferences.h"

static base::LazyInstance<PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor(base::LINKER_INITIALIZED);

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : TabContentsObserver(contents),
      delegate_(NULL),
      is_starred_(false),
      tab_contents_(contents) {
  DCHECK(contents);
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  // Create the tab helpers.
  autocomplete_history_manager_.reset(new AutocompleteHistoryManager(contents));
  autofill_manager_.reset(new AutofillManager(contents));
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
  download_tab_helper_.reset(new DownloadTabHelper(contents));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  find_tab_helper_.reset(new FindTabHelper(contents));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(contents));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  print_view_manager_.reset(new printing::PrintViewManager(contents));

  // Register for notifications about URL starredness changing on any profile.
  registrar_.Add(this, NotificationType::URLS_STARRED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 NotificationService::AllSources());

  // Create the per-tab observers.
  file_select_observer_.reset(new FileSelectObserver(contents));
  prerender_observer_.reset(new prerender::PrerenderObserver(contents));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
  webnavigation_observer_.reset(
      new ExtensionWebNavigationTabObserver(contents));
}

TabContentsWrapper::~TabContentsWrapper() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();
}

PropertyAccessor<TabContentsWrapper*>* TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

void TabContentsWrapper::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled, true);

  WebPreferences pref_defaults;
  prefs->RegisterBooleanPref(prefs::kWebKitJavascriptEnabled,
                             pref_defaults.javascript_enabled);
  prefs->RegisterBooleanPref(prefs::kWebKitWebSecurityEnabled,
                             pref_defaults.web_security_enabled);
  prefs->RegisterBooleanPref(
      prefs::kWebKitJavascriptCanOpenWindowsAutomatically, true);
  prefs->RegisterBooleanPref(prefs::kWebKitLoadsImagesAutomatically,
                             pref_defaults.loads_images_automatically);
  prefs->RegisterBooleanPref(prefs::kWebKitPluginsEnabled,
                             pref_defaults.plugins_enabled);
  prefs->RegisterBooleanPref(prefs::kWebKitDomPasteEnabled,
                             pref_defaults.dom_paste_enabled);
  prefs->RegisterBooleanPref(prefs::kWebKitShrinksStandaloneImagesToFit,
                             pref_defaults.shrinks_standalone_images_to_fit);
  prefs->RegisterDictionaryPref(prefs::kWebKitInspectorSettings);
  prefs->RegisterBooleanPref(prefs::kWebKitTextAreasAreResizable,
                             pref_defaults.text_areas_are_resizable);
  prefs->RegisterBooleanPref(prefs::kWebKitJavaEnabled,
                             pref_defaults.java_enabled);
  prefs->RegisterBooleanPref(prefs::kWebkitTabsToLinks,
                             pref_defaults.tabs_to_links);

  prefs->RegisterLocalizedStringPref(prefs::kAcceptLanguages,
                                     IDS_ACCEPT_LANGUAGES);
  prefs->RegisterLocalizedStringPref(prefs::kDefaultCharset,
                                     IDS_DEFAULT_ENCODING);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitStandardFontFamily,
                                     IDS_STANDARD_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFixedFontFamily,
                                     IDS_FIXED_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSerifFontFamily,
                                     IDS_SERIF_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitSansSerifFontFamily,
                                     IDS_SANS_SERIF_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitCursiveFontFamily,
                                     IDS_CURSIVE_FONT_FAMILY);
  prefs->RegisterLocalizedStringPref(prefs::kWebKitFantasyFontFamily,
                                     IDS_FANTASY_FONT_FAMILY);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFontSize,
                                      IDS_DEFAULT_FONT_SIZE);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitDefaultFixedFontSize,
                                      IDS_DEFAULT_FIXED_FONT_SIZE);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumFontSize,
                                      IDS_MINIMUM_FONT_SIZE);
  prefs->RegisterLocalizedIntegerPref(prefs::kWebKitMinimumLogicalFontSize,
                                      IDS_MINIMUM_LOGICAL_FONT_SIZE);
  prefs->RegisterLocalizedBooleanPref(prefs::kWebKitUsesUniversalDetector,
                                      IDS_USES_UNIVERSAL_DETECTOR);
  prefs->RegisterLocalizedStringPref(prefs::kStaticEncodings,
                                     IDS_STATIC_ENCODING_LIST);
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding, "");
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

TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    TabContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, TabContentsObserver implementation:

void TabContentsWrapper::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& /*details*/,
    const ViewHostMsg_FrameNavigate_Params& /*params*/) {
  UpdateStarredStateForCurrentURL();
}

bool TabContentsWrapper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabContentsWrapper, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PageContents, OnPageContents)
    IPC_MESSAGE_HANDLER(ViewHostMsg_JSOutOfMemory, OnJSOutOfMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterProtocolHandler,
                        OnRegisterProtocolHandler)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Thumbnail, OnMsgThumbnail)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, NotificationObserver implementation:

void TabContentsWrapper::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BOOKMARK_MODEL_LOADED:
      // BookmarkModel finished loading, fall through to update starred state.
    case NotificationType::URLS_STARRED: {
      // Somewhere, a URL has been starred.
      // Ignore notifications for profiles other than our current one.
      Profile* source_profile = Source<Profile>(source).ptr();
      if (!source_profile || !source_profile->IsSameProfile(profile()))
        return;

      UpdateStarredStateForCurrentURL();
      break;
    }

    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void TabContentsWrapper::OnPageContents(const GURL& url,
                                        int32 page_id,
                                        const string16& contents) {
  // Don't index any https pages. People generally don't want their bank
  // accounts, etc. indexed on their computer, especially since some of these
  // things are not marked cachable.
  // TODO(brettw) we may want to consider more elaborate heuristics such as
  // the cachability of the page. We may also want to consider subframes (this
  // test will still index subframes if the subframe is SSL).
  // TODO(zelidrag) bug chromium-os:2808 - figure out if we want to reenable
  // content indexing for chromeos in some future releases.
#if !defined(OS_CHROMEOS)
  if (!url.SchemeIsSecure()) {
    Profile* p = profile();
    if (p && !p->IsOffTheRecord()) {
      HistoryService* hs = p->GetHistoryService(Profile::IMPLICIT_ACCESS);
      if (hs)
        hs->SetPageContents(url, contents);
    }
  }
#endif
}

void TabContentsWrapper::OnJSOutOfMemory() {
  tab_contents()->AddInfoBar(new SimpleAlertInfoBarDelegate(tab_contents(),
      NULL, l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT), true));
}

void TabContentsWrapper::OnRegisterProtocolHandler(const std::string& protocol,
                                                   const GURL& url,
                                                   const string16& title) {
  ProtocolHandlerRegistry* registry = profile()->GetProtocolHandlerRegistry();
  ProtocolHandler* handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);
  if ((handler != NULL) &&
      registry->CanSchemeBeOverridden(handler->protocol())) {
    tab_contents()->AddInfoBar(registry->IsAlreadyRegistered(handler) ?
      static_cast<InfoBarDelegate*>(new SimpleAlertInfoBarDelegate(
          tab_contents(), NULL, l10n_util::GetStringFUTF16(
              IDS_REGISTER_PROTOCOL_HANDLER_ALREADY_REGISTERED,
              handler->title(), UTF8ToUTF16(handler->protocol())), true)) :
      new RegisterProtocolHandlerInfoBarDelegate(tab_contents(), registry,
                                                 handler));
  }
}

void TabContentsWrapper::OnMsgThumbnail(const GURL& url,
                                        const ThumbnailScore& score,
                                        const SkBitmap& bitmap) {
  if (profile()->IsOffTheRecord())
    return;

  // Tell History about this thumbnail
  history::TopSites* ts = profile()->GetTopSites();
  if (ts)
    ts->SetPageThumbnail(url, bitmap, score);
}

void TabContentsWrapper::UpdateStarredStateForCurrentURL() {
  BookmarkModel* model = tab_contents()->profile()->GetBookmarkModel();
  const bool old_state = is_starred_;
  is_starred_ = (model && model->IsBookmarked(tab_contents()->GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(this, is_starred_);
}
