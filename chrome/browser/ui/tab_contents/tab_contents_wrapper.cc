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
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter_observer.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
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
#include "chrome/browser/ui/intents/web_intent_picker_factory_impl.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/pdf/pdf_tab_observer.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sad_tab_observer.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/snapshot_tab_helper.h"
#include "chrome/browser/ui/sync/tab_contents_wrapper_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_switches.h"
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
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
  blocked_content_tab_helper_.reset(new BlockedContentTabHelper(this));
  bookmark_tab_helper_.reset(new BookmarkTabHelper(this));
  constrained_window_tab_helper_.reset(new ConstrainedWindowTabHelper(this));
  core_tab_helper_.reset(new CoreTabHelper(contents));
  extension_tab_helper_.reset(new ExtensionTabHelper(this));
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  infobar_tab_helper_.reset(new InfoBarTabHelper(contents));
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  prefs_tab_helper_.reset(new PrefsTabHelper(contents));
  prerender_tab_helper_.reset(new prerender::PrerenderTabHelper(this));
  print_view_manager_.reset(new printing::PrintViewManager(this));
  restore_tab_helper_.reset(new RestoreTabHelper(contents));
  search_engine_tab_helper_.reset(new SearchEngineTabHelper(contents));
  snapshot_tab_helper_.reset(new SnapshotTabHelper(contents));
  ssl_helper_.reset(new TabContentsSSLHelper(this));
  synced_tab_delegate_.reset(new TabContentsWrapperSyncedTabDelegate(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  web_intent_picker_controller_.reset(new WebIntentPickerController(
      this, new WebIntentPickerFactoryImpl()));

  // Create the per-tab observers.
  alternate_error_page_tab_observer_.reset(
      new AlternateErrorPageTabObserver(contents));
  download_request_limiter_observer_.reset(
      new DownloadRequestLimiterObserver(contents));
  webnavigation_observer_.reset(
      new ExtensionWebNavigationTabObserver(contents));
  external_protocol_observer_.reset(new ExternalProtocolObserver(contents));
  if (OmniboxSearchHint::IsEnabled(profile()))
    omnibox_search_hint_.reset(new OmniboxSearchHint(this));
  pdf_tab_observer_.reset(new PDFTabObserver(this));
  plugin_observer_.reset(new PluginObserver(this));
  print_preview_.reset(new printing::PrintPreviewMessageHandler(contents));
  sad_tab_observer_.reset(new SadTabObserver(contents));
  safe_browsing_tab_observer_.reset(
      new safe_browsing::SafeBrowsingTabObserver(this));

  // Start the in-browser thumbnailing if the feature is enabled.
  if (switches::IsInBrowserThumbnailingEnabled()) {
    thumbnail_generation_observer_.reset(new ThumbnailGenerator);
    thumbnail_generation_observer_->StartThumbnailing(web_contents_.get());
  }
}

TabContentsWrapper::~TabContentsWrapper() {
  in_destructor_ = true;

  // Need to tear down infobars before the TabContents goes away.
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
  // will attempt to access the TabContents and we'll crash.
  DCHECK(in_destructor_);
}
