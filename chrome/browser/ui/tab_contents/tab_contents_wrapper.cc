// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter_observer.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/page_action_controller.h"
#include "chrome/browser/extensions/script_executor_impl.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/pdf/pdf_tab_observer.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/snapshot_tab_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/tab_contents_wrapper_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_view_type.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

static base::LazyInstance<base::PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor = LAZY_INSTANCE_INITIALIZER;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(WebContents* contents)
    : content::WebContentsObserver(contents),
      in_destructor_(false),
      web_contents_(contents) {
  DCHECK(contents);
  DCHECK(!GetCurrentWrapperForContents(contents));

  web_contents_->SetViewType(chrome::VIEW_TYPE_TAB_CONTENTS);

  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->GetPropertyBag(), this);

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
#if defined(ENABLE_AUTOMATION)
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
#endif
  blocked_content_tab_helper_.reset(new BlockedContentTabHelper(this));
  bookmark_tab_helper_.reset(new BookmarkTabHelper(this));
  constrained_window_tab_helper_.reset(new ConstrainedWindowTabHelper(this));
  core_tab_helper_.reset(new CoreTabHelper(contents));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  extension_script_executor_.reset(
      new extensions::ScriptExecutorImpl(web_contents()));
  extension_action_box_controller_.reset(
      new extensions::PageActionController(this));
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  hung_plugin_tab_helper_.reset(new HungPluginTabHelper(contents));
  infobar_tab_helper_.reset(new InfoBarTabHelper(contents));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  prefs_tab_helper_.reset(new PrefsTabHelper(contents));
  prerender_tab_helper_.reset(new prerender::PrerenderTabHelper(this));
  restore_tab_helper_.reset(new RestoreTabHelper(contents));
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  snapshot_tab_helper_.reset(new SnapshotTabHelper(contents));
  ssl_helper_.reset(new TabContentsSSLHelper(this));
  synced_tab_delegate_.reset(new TabContentsWrapperSyncedTabDelegate(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));

#if defined(ENABLE_WEB_INTENTS)
  web_intent_picker_controller_.reset(new WebIntentPickerController(this));
#endif

#if !defined(OS_ANDROID)
  print_view_manager_.reset(new printing::PrintViewManager(this));
  sad_tab_helper_.reset(new SadTabHelper(contents));
#endif

  // Create the per-tab observers.
  alternate_error_page_tab_observer_.reset(
      new AlternateErrorPageTabObserver(contents, profile()));
  download_request_limiter_observer_.reset(
      new DownloadRequestLimiterObserver(contents));
  webnavigation_observer_.reset(
      new extensions::WebNavigationTabObserver(contents));
  external_protocol_observer_.reset(new ExternalProtocolObserver(contents));
  pdf_tab_observer_.reset(new PDFTabObserver(this));
  plugin_observer_.reset(new PluginObserver(this));
  safe_browsing_tab_observer_.reset(
      new safe_browsing::SafeBrowsingTabObserver(this));

#if !defined(OS_ANDROID)
  if (OmniboxSearchHint::IsEnabled(profile()))
    omnibox_search_hint_.reset(new OmniboxSearchHint(this));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
#endif

  // Start the in-browser thumbnailing if the feature is enabled.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInBrowserThumbnailing)) {
    thumbnail_generator_.reset(new ThumbnailGenerator);
    thumbnail_generator_->StartThumbnailing(web_contents_.get());
  }

  // If this is not an incognito window, setup to handle one-click login.
  // We don't want to check that the profile is already connected at this time
  // because the connected state may change while this tab is open.  Having a
  // one-click signin helper attached does not cause problems if the profile
  // happens to be already connected.
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  if (OneClickSigninHelper::CanOffer(contents, false))
      one_click_signin_helper_.reset(new OneClickSigninHelper(contents));
#endif
}

TabContentsWrapper::~TabContentsWrapper() {
  in_destructor_ = true;

  // Need to tear down infobars before the WebContents goes away.
  // TODO(avi): Can we get this handled by the tab helper itself?
  infobar_tab_helper_.reset();
}

base::PropertyAccessor<TabContentsWrapper*>*
    TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

TabContentsWrapper* TabContentsWrapper::Clone() {
  WebContents* new_contents = web_contents()->Clone();
  TabContentsWrapper* new_wrapper = new TabContentsWrapper(new_contents);

  // TODO(avi): Can we generalize this so that knowledge of the functionings of
  // the tab helpers isn't required here?
  new_wrapper->extension_tab_helper()->CopyStateFrom(
      *extension_tab_helper_.get());
  return new_wrapper;
}

// static
TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    WebContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->GetPropertyBag());

  return wrapper ? *wrapper : NULL;
}

// static
const TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    const WebContents* contents) {
  TabContentsWrapper* const* wrapper =
      property_accessor()->GetProperty(contents->GetPropertyBag());

  return wrapper ? *wrapper : NULL;
}

WebContents* TabContentsWrapper::web_contents() const {
  return web_contents_.get();
}

Profile* TabContentsWrapper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void TabContentsWrapper::WebContentsDestroyed(WebContents* tab) {
  // Destruction of the WebContents should only be done by us from our
  // destructor. Otherwise it's very likely we (or one of the helpers we own)
  // will attempt to access the WebContents and we'll crash.
  DCHECK(in_destructor_);
}
