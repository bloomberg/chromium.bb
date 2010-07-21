// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TESTING_PROFILE_H_
#define CHROME_TEST_TESTING_PROFILE_H_

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/find_bar_state.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/pref_value_store.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/testing_pref_service.h"
#include "net/base/cookie_monster.h"

class ProfileSyncService;
class SessionService;

class TestingProfile : public Profile {
 public:
  TestingProfile();

  // Creates a new profile by adding |count| to the end of the path. Use this
  // when you need to have more than one TestingProfile running at the same
  // time.
  explicit TestingProfile(int count);

  virtual ~TestingProfile();

  // Creates the favicon service. Consequent calls would recreate the service.
  void CreateFaviconService();

  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As TestingProfile
  // deletes the directory containing the files used by HistoryService, this
  // only matters if you're recreating the HistoryService.  If |no_db| is true,
  // the history backend will fail to initialize its database; this is useful
  // for testing error conditions.
  void CreateHistoryService(bool delete_file, bool no_db);

  // Creates the BookmkarBarModel. If not invoked the bookmark bar model is
  // NULL. If |delete_file| is true, the bookmarks file is deleted first, then
  // the model is created. As TestingProfile deletes the directory containing
  // the files used by HistoryService, the boolean only matters if you're
  // recreating the BookmarkModel.
  //
  // NOTE: this does not block until the bookmarks are loaded. For that use
  // BlockUntilBookmarkModelLoaded.
  void CreateBookmarkModel(bool delete_file);

  // Creates an AutocompleteClassifier. If not invoked the
  // AutocompleteClassifier is NULL.
  void CreateAutocompleteClassifier();

  // Creates the webdata service.  If |delete_file| is true, the webdata file is
  // deleted first, then the WebDataService is created.  As TestingProfile
  // deletes the directory containing the files used by WebDataService, this
  // only matters if you're recreating the WebDataService.
  void CreateWebDataService(bool delete_file);

  // Blocks until the BookmarkModel finishes loaded. This is NOT invoked from
  // CreateBookmarkModel.
  void BlockUntilBookmarkModelLoaded();

  // Creates a TemplateURLModel. If not invoked the TemplateURLModel is NULL.
  void CreateTemplateURLModel();

  // Uses a specific theme provider for this profile. TestingProfile takes
  // ownership of |theme_provider|.
  void UseThemeProvider(BrowserThemeProvider* theme_provider);

  virtual ProfileId GetRuntimeId() {
    return reinterpret_cast<ProfileId>(this);
  }

  virtual FilePath GetPath() {
    return path_;
  }
  // Sets whether we're off the record. Default is false.
  void set_off_the_record(bool off_the_record) {
    off_the_record_ = off_the_record;
  }
  virtual bool IsOffTheRecord() { return off_the_record_; }
  virtual Profile* GetOffTheRecordProfile() { return NULL; }

  virtual void DestroyOffTheRecordProfile() {}

  virtual bool HasOffTheRecordProfile() { return false; }

  virtual Profile* GetOriginalProfile() { return this; }
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker();
  virtual VisitedLinkMaster* GetVisitedLinkMaster() { return NULL; }
  virtual ExtensionsService* GetExtensionsService() { return NULL; }
  virtual UserScriptMaster* GetUserScriptMaster() { return NULL; }
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() {
    return NULL;
  }
  virtual ExtensionProcessManager* GetExtensionProcessManager() { return NULL; }
  virtual ExtensionMessageService* GetExtensionMessageService() { return NULL; }
  virtual SSLHostState* GetSSLHostState() { return NULL; }
  virtual net::TransportSecurityState* GetTransportSecurityState() {
    return NULL;
  }
  virtual FaviconService* GetFaviconService(ServiceAccessType access) {
    return favicon_service_.get();
  }
  virtual HistoryService* GetHistoryService(ServiceAccessType access) {
    return history_service_.get();
  }
  virtual HistoryService* GetHistoryServiceWithoutCreating() {
    return history_service_.get();
  }
  void set_has_history_service(bool has_history_service) {
    has_history_service_ = has_history_service;
  }
  // The CookieMonster will only be returned if a Context has been created. Do
  // this by calling CreateRequestContext(). See the note at GetRequestContext
  // for more information.
  net::CookieMonster* GetCookieMonster() {
    if (!GetRequestContext())
      return NULL;
    return GetRequestContext()->GetCookieStore()->GetCookieMonster();
  }
  virtual AutocompleteClassifier* GetAutocompleteClassifier() {
    return autocomplete_classifier_.get();
  }
  virtual WebDataService* GetWebDataService(ServiceAccessType access) {
    return web_data_service_.get();
  }
  virtual WebDataService* GetWebDataServiceWithoutCreating() {
    return web_data_service_.get();
  }
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access) {
    return NULL;
  }
  virtual TestingPrefService* GetPrefs() {
    if (!prefs_.get()) {
      prefs_.reset(new TestingPrefService());
      Profile::RegisterUserPrefs(prefs_.get());
      browser::RegisterAllPrefs(prefs_.get(), prefs_.get());
    }
    return prefs_.get();
  }
  virtual TemplateURLModel* GetTemplateURLModel() {
    return template_url_model_.get();
  }
  virtual TemplateURLFetcher* GetTemplateURLFetcher() { return NULL; }
  virtual history::TopSites* GetTopSites() { return NULL; }
  virtual DownloadManager* GetDownloadManager() { return NULL; }
  virtual PersonalDataManager* GetPersonalDataManager() { return NULL; }
  virtual bool HasCreatedDownloadManager() const { return false; }
  virtual void InitThemes();
  virtual void SetTheme(Extension* extension) {}
  virtual void SetNativeTheme() {}
  virtual void ClearTheme() {}
  virtual Extension* GetTheme() { return NULL; }
  virtual BrowserThemeProvider* GetThemeProvider() {
    InitThemes();
    return theme_provider_.get();
  }

  // Returns a testing ContextGetter (if one has been created via
  // CreateRequestContext) or NULL. This is not done on-demand for two reasons:
  // (1) Some tests depend on GetRequestContext() returning NULL. (2) Because
  // of the special memory management considerations for the
  // TestURLRequestContextGetter class, many tests would find themseleves
  // leaking if they called this method without the necessary IO thread. This
  // getter is currently only capable of returning a Context that helps test
  // the CookieMonster. See implementation comments for more details.
  virtual URLRequestContextGetter* GetRequestContext();
  void CreateRequestContext();

  virtual URLRequestContextGetter* GetRequestContextForMedia() { return NULL; }
  virtual URLRequestContextGetter* GetRequestContextForExtensions();

  virtual net::SSLConfigService* GetSSLConfigService() { return NULL; }
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher() { return NULL; }
  virtual FindBarState* GetFindBarState() {
    if (!find_bar_state_.get())
      find_bar_state_.reset(new FindBarState());
    return find_bar_state_.get();
  }
  virtual HostContentSettingsMap* GetHostContentSettingsMap() {
    if (!host_content_settings_map_.get())
      host_content_settings_map_ = new HostContentSettingsMap(this);
    return host_content_settings_map_.get();
  }
  virtual GeolocationContentSettingsMap* GetGeolocationContentSettingsMap() {
    if (!geolocation_content_settings_map_.get()) {
      geolocation_content_settings_map_ =
          new GeolocationContentSettingsMap(this);
    }
    return geolocation_content_settings_map_.get();
  }
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext() {
    if (!geolocation_permission_context_.get()) {
      geolocation_permission_context_ =
          new GeolocationPermissionContext(this);
    }
    return geolocation_permission_context_.get();
  }
  virtual HostZoomMap* GetHostZoomMap() { return NULL; }
  void set_session_service(SessionService* session_service);
  virtual SessionService* GetSessionService() { return session_service_.get(); }
  virtual void ShutdownSessionService() {}
  virtual bool HasSessionService() const {
    return (session_service_.get() != NULL);
  }
  virtual std::wstring GetName() { return std::wstring(); }
  virtual void SetName(const std::wstring& name) {}
  virtual std::wstring GetID() { return id_; }
  virtual void SetID(const std::wstring& id) { id_ = id; }
  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  virtual bool DidLastSessionExitCleanly() {
    return last_session_exited_cleanly_;
  }
  virtual void MergeResourceString(int message_id,
                                   std::wstring* output_string) {}
  virtual void MergeResourceInteger(int message_id, int* output_value) {}
  virtual void MergeResourceBoolean(int message_id, bool* output_value) {}
  virtual BookmarkModel* GetBookmarkModel() {
    return bookmark_bar_model_.get();
  }
  virtual bool IsSameProfile(Profile *p) { return this == p; }
  virtual base::Time GetStartTime() const { return start_time_; }
  virtual TabRestoreService* GetTabRestoreService() { return NULL; }
  virtual void ResetTabRestoreService() {}
  virtual SpellCheckHost* GetSpellCheckHost() { return NULL; }
  virtual void ReinitializeSpellCheckHost(bool force) { }
  virtual WebKitContext* GetWebKitContext() {
    if (webkit_context_ == NULL)
      webkit_context_ = new WebKitContext(this);
    return webkit_context_;
  }
  virtual WebKitContext* GetOffTheRecordWebKitContext() { return NULL; }
  virtual void MarkAsCleanShutdown() {}
  virtual void InitExtensions() {}
  virtual void InitWebResources() {}
  virtual NTPResourceCache* GetNTPResourceCache();
  virtual DesktopNotificationService* GetDesktopNotificationService() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    if (!desktop_notification_service_.get()) {
       desktop_notification_service_.reset(new DesktopNotificationService(
           this, NULL));
    }
    return desktop_notification_service_.get();
  }
  virtual BackgroundContentsService* GetBackgroundContentsService() {
    return NULL;
  }
  virtual FilePath last_selected_directory() {
    return last_selected_directory_;
  }
  virtual void set_last_selected_directory(const FilePath& path) {
    last_selected_directory_ = path;
  }


  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  // Creates and initializes a profile sync service if the tests require one.
  virtual TokenService* GetTokenService();
  virtual ProfileSyncService* GetProfileSyncService();
  virtual CloudPrintProxyService* GetCloudPrintProxyService() { return NULL; }

 protected:
  // The path of the profile; the various database and other files are relative
  // to this.
  FilePath path_;
  base::Time start_time_;
  scoped_ptr<TestingPrefService> prefs_;

 private:
  // Destroys favicon service if it has been created.
  void DestroyFaviconService();

  // If the history service has been created, it is destroyed. This is invoked
  // from the destructor.
  void DestroyHistoryService();

  // If the webdata service has been created, it is destroyed.  This is invoked
  // from the destructor.
  void DestroyWebDataService();

  // The favicon service. Only created if CreateFaviconService is invoked.
  scoped_refptr<FaviconService> favicon_service_;

  // The history service. Only created if CreateHistoryService is invoked.
  scoped_refptr<HistoryService> history_service_;

  // The BookmarkModel. Only created if CreateBookmarkModel is invoked.
  scoped_ptr<BookmarkModel> bookmark_bar_model_;

  // The TokenService. Created by CreateTokenService. Filled with dummy data.
  scoped_ptr<TokenService> token_service_;

  // The ProfileSyncService.  Created by CreateProfileSyncService.
  scoped_ptr<ProfileSyncService> profile_sync_service_;

  // The AutocompleteClassifier.  Only created if CreateAutocompleteClassifier
  // is invoked.
  scoped_ptr<AutocompleteClassifier> autocomplete_classifier_;

  // The WebDataService.  Only created if CreateWebDataService is invoked.
  scoped_refptr<WebDataService> web_data_service_;

  // The TemplateURLFetcher. Only created if CreateTemplateURLModel is invoked.
  scoped_ptr<TemplateURLModel> template_url_model_;

  scoped_ptr<NTPResourceCache> ntp_resource_cache_;

  // The SessionService. Defaults to NULL, but can be set using the setter.
  scoped_refptr<SessionService> session_service_;

  // The theme provider. Created lazily by GetThemeProvider()/InitThemes().
  scoped_ptr<BrowserThemeProvider> theme_provider_;
  bool created_theme_provider_;

  // Internally, this is a TestURLRequestContextGetter that creates a dummy
  // request context. Currently, only the CookieMonster is hooked up.
  scoped_refptr<URLRequestContextGetter> request_context_;
  scoped_refptr<URLRequestContextGetter> extensions_request_context_;

  // Do we have a history service? This defaults to the value of
  // history_service, but can be explicitly set.
  bool has_history_service_;

  std::wstring id_;

  bool off_the_record_;

  // Did the last session exit cleanly? Default is true.
  bool last_session_exited_cleanly_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  // WebKitContext, lazily initialized by GetWebKitContext().
  scoped_refptr<WebKitContext> webkit_context_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<GeolocationContentSettingsMap>
      geolocation_content_settings_map_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;
  scoped_ptr<DesktopNotificationService> desktop_notification_service_;

  // Find bar state.  Created lazily by GetFindBarState().
  scoped_ptr<FindBarState> find_bar_state_;

  FilePath last_selected_directory_;
};

// A profile that derives from another profile.  This does not actually
// override anything except the GetRuntimeId() in order to test sharing of
// site information.
class DerivedTestingProfile : public TestingProfile {
 public:
  explicit DerivedTestingProfile(Profile* profile)
      : original_profile_(profile) {}

  virtual ProfileId GetRuntimeId() {
    return original_profile_->GetRuntimeId();
  }

 protected:
  Profile* original_profile_;
};

#endif  // CHROME_TEST_TESTING_PROFILE_H_
