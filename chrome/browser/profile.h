// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILE_H_
#define CHROME_BROWSER_PROFILE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/spellcheck_host_observer.h"
#include "chrome/common/notification_registrar.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#endif

namespace net {
class TransportSecurityState;
class SSLConfigService;
}

namespace webkit_database {
class DatabaseTracker;
}

class BlacklistManager;
class BookmarkModel;
class BrowserThemeProvider;
class ChromeURLRequestContextGetter;
class DesktopNotificationService;
class DownloadManager;
class Extension;
class ExtensionDevToolsManager;
class ExtensionProcessManager;
class ExtensionMessageService;
class ExtensionsService;
class FaviconService;
class HistoryService;
class HostZoomMap;
class NavigationController;
class NTPResourceCache;
class PasswordStore;
class PersonalDataManager;
class PrefService;
class ProfileSyncService;
class SearchVersusNavigateClassifier;
class SessionService;
class SpellCheckHost;
class SSLConfigServiceManager;
class SSLHostState;
class TransportSecurityPersister;
class SQLitePersistentCookieStore;
class TabRestoreService;
class TemplateURLFetcher;
class TemplateURLModel;
class ThemeProvider;
class ThumbnailStore;
class URLRequestContextGetter;
class UserScriptMaster;
class VisitedLinkCreator;
class VisitedLinkMaster;
class WebDataService;
class WebKitContext;
class WebResourceService;

typedef intptr_t ProfileId;

class Profile {
 public:
  // Profile services are accessed with the following parameter. This parameter
  // defines what the caller plans to do with the service.
  // The caller is responsible for not performing any operation that would
  // result in persistent implicit records while using an OffTheRecord profile.
  // This flag allows the profile to perform an additional check.
  //
  // It also gives us an opportunity to perform further checks in the future. We
  // could, for example, return an history service that only allow some specific
  // methods.
  enum ServiceAccessType {
    // The caller plans to perform a read or write that takes place as a result
    // of the user input. Use this flag when the operation you are doing can be
    // performed while off the record. (ex: creating a bookmark)
    //
    // Since EXPLICIT_ACCESS means "as a result of a user action", this request
    // always succeeds.
    EXPLICIT_ACCESS,

    // The caller plans to call a method that will permanently change some data
    // in the profile, as part of Chrome's implicit data logging. Use this flag
    // when you are about to perform an operation which is incompatible with the
    // off the record mode.
    IMPLICIT_ACCESS
  };

  // Value that represents no profile Id.
  static const ProfileId InvalidProfileId;

  Profile() : restored_last_session_(false) {}
  virtual ~Profile() {}

  // Profile prefs are registered as soon as the prefs are loaded for the first
  // time.
  static void RegisterUserPrefs(PrefService* prefs);

  // Create a new profile given a path.
  static Profile* CreateProfile(const FilePath& path);

  // Returns the request context for the "default" profile.  This may be called
  // from any thread.  This CAN return NULL if a first request context has not
  // yet been created.  If necessary, listen on the UI thread for
  // NOTIFY_DEFAULT_REQUEST_CONTEXT_AVAILABLE.
  static URLRequestContextGetter* GetDefaultRequestContext();

  // Returns a unique Id that can be used to identify this profile at runtime.
  // This Id is not persistent and will not survive a restart of the browser.
  virtual ProfileId GetRuntimeId() = 0;

  // Returns the path of the directory where this profile's data is stored.
  virtual FilePath GetPath() = 0;

  // Return whether this profile is off the record. Default is false.
  virtual bool IsOffTheRecord() = 0;

  // Return the off the record version of this profile. The returned pointer
  // is owned by the receiving profile. If the receiving profile is off the
  // record, the same profile is returned.
  virtual Profile* GetOffTheRecordProfile() = 0;

  // Destroys the off the record profile.
  virtual void DestroyOffTheRecordProfile() = 0;

  // Return the original "recording" profile. This method returns this if the
  // profile is not off the record.
  virtual Profile* GetOriginalProfile() = 0;

  // Returns a pointer to the DatabaseTracker instance for this profile.
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() = 0;

  // Retrieves a pointer to the VisitedLinkMaster associated with this
  // profile.  The VisitedLinkMaster is lazily created the first time
  // that this method is called.
  virtual VisitedLinkMaster* GetVisitedLinkMaster() = 0;

  // Loads the visited link master on the file thread.  It's safe to call
  // GetVisitedLinkMaster without calling this in advance.
  virtual void PreloadVisitedLinkMaster() = 0;

  // Retrieves a pointer to the ExtensionsService associated with this
  // profile. The ExtensionsService is created at startup.
  virtual ExtensionsService* GetExtensionsService() = 0;

  // Retrieves a pointer to the UserScriptMaster associated with this
  // profile.  The UserScriptMaster is lazily created the first time
  // that this method is called.
  virtual UserScriptMaster* GetUserScriptMaster() = 0;

  // Retrieves a pointer to the ExtensionDevToolsManager associated with this
  // profile.  The instance is created at startup.
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() = 0;

  // Retrieves a pointer to the ExtensionProcessManager associated with this
  // profile.  The instance is created at startup.
  virtual ExtensionProcessManager* GetExtensionProcessManager() = 0;

  // Retrieves a pointer to the ExtensionMessageService associated with this
  // profile.  The instance is created at startup.
  virtual ExtensionMessageService* GetExtensionMessageService() = 0;

  // Retrieves a pointer to the SSLHostState associated with this profile.
  // The SSLHostState is lazily created the first time that this method is
  // called.
  virtual SSLHostState* GetSSLHostState() = 0;

  // Retrieves a pointer to the TransportSecurityState associated with
  // this profile.  The TransportSecurityState is lazily created the
  // first time that this method is called.
  virtual net::TransportSecurityState*
      GetTransportSecurityState() = 0;

  // Retrieves a pointer to the FaviconService associated with this
  // profile.  The FaviconService is lazily created the first time
  // that this method is called.
  //
  // Although FaviconService is refcounted, this will not addref, and callers
  // do not need to do any reference counting as long as they keep the pointer
  // only for the local scope (which they should do anyway since the browser
  // process may decide to shut down).
  //
  // |access| defines what the caller plans to do with the service. See
  // the ServiceAccessType definition above.
  virtual FaviconService* GetFaviconService(ServiceAccessType access) = 0;

  // Retrieves a pointer to the HistoryService associated with this
  // profile.  The HistoryService is lazily created the first time
  // that this method is called.
  //
  // Although HistoryService is refcounted, this will not addref, and callers
  // do not need to do any reference counting as long as they keep the pointer
  // only for the local scope (which they should do anyway since the browser
  // process may decide to shut down).
  //
  // |access| defines what the caller plans to do with the service. See
  // the ServiceAccessType definition above.
  virtual HistoryService* GetHistoryService(ServiceAccessType access) = 0;

  // Similar to GetHistoryService(), but won't create the history service if it
  // doesn't already exist.
  virtual HistoryService* GetHistoryServiceWithoutCreating() = 0;

  // Retrieves a pointer to the SearchVersusNavigateClassifier associated with
  // this profile. The SearchVersusNavigateClassifier is lazily created the
  // first time that this method is called.
  virtual SearchVersusNavigateClassifier*
      GetSearchVersusNavigateClassifier() = 0;

  // Returns the WebDataService for this profile. This is owned by
  // the Profile. Callers that outlive the life of this profile need to be
  // sure they refcount the returned value.
  //
  // |access| defines what the caller plans to do with the service. See
  // the ServiceAccessType definition above.
  virtual WebDataService* GetWebDataService(ServiceAccessType access) = 0;

  // Similar to GetWebDataService(), but won't create the web data service if it
  // doesn't already exist.
  virtual WebDataService* GetWebDataServiceWithoutCreating() = 0;

  // Returns the PasswordStore for this profile. This is owned by the Profile.
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access) = 0;

  // Retrieves a pointer to the PrefService that manages the preferences
  // for this user profile.  The PrefService is lazily created the first
  // time that this method is called.
  virtual PrefService* GetPrefs() = 0;

  // Returns the TemplateURLModel for this profile. This is owned by the
  // the Profile.
  virtual TemplateURLModel* GetTemplateURLModel() = 0;

  // Returns the TemplateURLFetcher for this profile. This is owned by the
  // profile.
  virtual TemplateURLFetcher* GetTemplateURLFetcher() = 0;

  // Returns the DownloadManager associated with this profile.
  virtual DownloadManager* GetDownloadManager() = 0;
  virtual bool HasCreatedDownloadManager() const = 0;

  // Returns the PersonalDataManager associated with this profile.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;

  // Init our themes system.
  virtual void InitThemes() = 0;

  // Set the theme to the specified extension.
  virtual void SetTheme(Extension* extension) = 0;

  // Set the theme to the machine's native theme.
  virtual void SetNativeTheme() = 0;

  // Clear the theme and reset it to default.
  virtual void ClearTheme() = 0;

  // Gets the theme that was last set. Returns NULL if the theme is no longer
  // installed, if there is no installed theme, or the theme was cleared.
  virtual Extension* GetTheme() = 0;

  // Returns or creates the ThemeProvider associated with this profile
  virtual ThemeProvider* GetThemeProvider() = 0;

  virtual ThumbnailStore* GetThumbnailStore() = 0;

  // Returns the request context information associated with this profile.  Call
  // this only on the UI thread, since it can send notifications that should
  // happen on the UI thread.
  virtual URLRequestContextGetter* GetRequestContext() = 0;

  // Returns the request context for media resources asociated with this
  // profile.
  virtual URLRequestContextGetter* GetRequestContextForMedia() = 0;

  // Returns the request context used for extension-related requests.  This
  // is only used for a separate cookie store currently.
  virtual URLRequestContextGetter* GetRequestContextForExtensions() = 0;

  // Returns the SSLConfigService for this profile.
  virtual net::SSLConfigService* GetSSLConfigService() = 0;

  // Returns the Hostname <-> Zoom Level map for this profile.
  virtual HostZoomMap* GetHostZoomMap() = 0;

  // Returns the Privacy Blacklist Manager for this profile.
  virtual BlacklistManager* GetBlacklistManager() = 0;

  // Returns the session service for this profile. This may return NULL. If
  // this profile supports a session service (it isn't off the record), and
  // the session service hasn't yet been created, this forces creation of
  // the session service.
  //
  // This returns NULL in two situations: the profile is off the record, or the
  // session service has been explicitly shutdown (browser is exiting). Callers
  // should always check the return value for NULL.
  virtual SessionService* GetSessionService() = 0;

  // If this profile has a session service, it is shut down. To properly record
  // the current state this forces creation of the session service, then shuts
  // it down.
  virtual void ShutdownSessionService() = 0;

  // Returns true if this profile has a session service.
  virtual bool HasSessionService() const = 0;

  // Returns true if the last time this profile was open it was exited cleanly.
  virtual bool DidLastSessionExitCleanly() = 0;

  // Returns the BookmarkModel, creating if not yet created.
  virtual BookmarkModel* GetBookmarkModel() = 0;

  // Returns the ProfileSyncService, creating if not yet created.
  virtual ProfileSyncService* GetProfileSyncService() = 0;

  // Return whether 2 profiles are the same. 2 profiles are the same if they
  // represent the same profile. This can happen if there is pointer equality
  // or if one profile is the off the record version of another profile (or vice
  // versa).
  virtual bool IsSameProfile(Profile* profile) = 0;

  // Returns the time the profile was started. This is not the time the profile
  // was created, rather it is the time the user started chrome and logged into
  // this profile. For the single profile case, this corresponds to the time
  // the user started chrome.
  virtual base::Time GetStartTime() const = 0;

  // Returns the TabRestoreService. This returns NULL when off the record.
  virtual TabRestoreService* GetTabRestoreService() = 0;

  virtual void ResetTabRestoreService() = 0;

  // May return NULL.
  virtual SpellCheckHost* GetSpellCheckHost() = 0;

  // If |force| is false, and the spellchecker is already initialized (or is in
  // the process of initializing), then do nothing. Otherwise clobber the
  // current spellchecker and replace it with a new one.
  virtual void ReinitializeSpellCheckHost(bool force) = 0;

  // Returns the WebKitContext assigned to this profile.
  virtual WebKitContext* GetWebKitContext() = 0;

  // Returns the provider of desktop notifications for this profile.
  virtual DesktopNotificationService* GetDesktopNotificationService() = 0;

  // Marks the profile as cleanly shutdown.
  //
  // NOTE: this is invoked internally on a normal shutdown, but is public so
  // that it can be invoked when the user logs out/powers down (WM_ENDSESSION).
  virtual void MarkAsCleanShutdown() = 0;

  virtual void InitExtensions() = 0;

  // Start up service that gathers data from web resource feeds.
  virtual void InitWebResources() = 0;

  // Returns the new tab page resource cache.
  virtual NTPResourceCache* GetNTPResourceCache() = 0;

#ifdef UNIT_TEST
  // Use with caution.  GetDefaultRequestContext may be called on any thread!
  static void set_default_request_context(URLRequestContextGetter* c) {
    default_request_context_ = c;
  }
#endif

  // Did the user restore the last session? This is set by SessionRestore.
  void set_restored_last_session(bool restored_last_session) {
    restored_last_session_ = restored_last_session;
  }
  bool restored_last_session() const {
    return restored_last_session_;
  }

 protected:
  static URLRequestContextGetter* default_request_context_;

 private:
  bool restored_last_session_;
};

class OffTheRecordProfileImpl;

// The default profile implementation.
class ProfileImpl : public Profile,
                    public SpellCheckHostObserver,
                    public NotificationObserver {
 public:
  virtual ~ProfileImpl();

  // Profile implementation.
  virtual ProfileId GetRuntimeId();
  virtual FilePath GetPath();
  virtual bool IsOffTheRecord();
  virtual Profile* GetOffTheRecordProfile();
  virtual void DestroyOffTheRecordProfile();
  virtual Profile* GetOriginalProfile();
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker();
  virtual VisitedLinkMaster* GetVisitedLinkMaster();
  virtual void PreloadVisitedLinkMaster();
  virtual UserScriptMaster* GetUserScriptMaster();
  virtual SSLHostState* GetSSLHostState();
  virtual net::TransportSecurityState* GetTransportSecurityState();
  virtual ExtensionsService* GetExtensionsService();
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager();
  virtual ExtensionProcessManager* GetExtensionProcessManager();
  virtual ExtensionMessageService* GetExtensionMessageService();
  virtual FaviconService* GetFaviconService(ServiceAccessType sat);
  virtual HistoryService* GetHistoryService(ServiceAccessType sat);
  virtual HistoryService* GetHistoryServiceWithoutCreating();
  virtual SearchVersusNavigateClassifier* GetSearchVersusNavigateClassifier();
  virtual WebDataService* GetWebDataService(ServiceAccessType sat);
  virtual WebDataService* GetWebDataServiceWithoutCreating();
  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat);
  virtual PrefService* GetPrefs();
  virtual TemplateURLModel* GetTemplateURLModel();
  virtual TemplateURLFetcher* GetTemplateURLFetcher();
  virtual DownloadManager* GetDownloadManager();
  virtual PersonalDataManager* GetPersonalDataManager();
  virtual void InitThemes();
  virtual void SetTheme(Extension* extension);
  virtual void SetNativeTheme();
  virtual void ClearTheme();
  virtual Extension* GetTheme();
  virtual ThemeProvider* GetThemeProvider();
  virtual ThumbnailStore* GetThumbnailStore();
  virtual bool HasCreatedDownloadManager() const;
  virtual URLRequestContextGetter* GetRequestContext();
  virtual URLRequestContextGetter* GetRequestContextForMedia();
  virtual URLRequestContextGetter* GetRequestContextForExtensions();
  virtual net::SSLConfigService* GetSSLConfigService();
  virtual HostZoomMap* GetHostZoomMap();
  virtual BlacklistManager* GetBlacklistManager();
  virtual SessionService* GetSessionService();
  virtual void ShutdownSessionService();
  virtual bool HasSessionService() const;
  virtual bool DidLastSessionExitCleanly();
  virtual BookmarkModel* GetBookmarkModel();
  virtual bool IsSameProfile(Profile* profile);
  virtual base::Time GetStartTime() const;
  virtual TabRestoreService* GetTabRestoreService();
  virtual void ResetTabRestoreService();
  virtual SpellCheckHost* GetSpellCheckHost();
  virtual void ReinitializeSpellCheckHost(bool force);
  virtual WebKitContext* GetWebKitContext();
  virtual DesktopNotificationService* GetDesktopNotificationService();
  virtual void MarkAsCleanShutdown();
  virtual void InitExtensions();
  virtual void InitWebResources();
  virtual NTPResourceCache* GetNTPResourceCache();
  virtual ProfileSyncService* GetProfileSyncService();
  void InitSyncService();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // SpellCheckHostObserver implementation.
  virtual void SpellCheckHostInitialized();

 private:
  friend class Profile;

  explicit ProfileImpl(const FilePath& path);

  void CreateWebDataService();
  FilePath GetPrefFilePath();

  void CreatePasswordStore();

  void StopCreateSessionServiceTimer();

  void EnsureRequestContextCreated() {
    GetRequestContext();
  }

  void EnsureSessionServiceCreated() {
    GetSessionService();
  }

  NotificationRegistrar registrar_;

  FilePath path_;
  FilePath base_cache_path_;
  scoped_refptr<VisitedLinkCreator> visited_link_creator_;
  scoped_refptr<ExtensionsService> extensions_service_;
  scoped_refptr<UserScriptMaster> user_script_master_;
  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;
  scoped_ptr<ExtensionProcessManager> extension_process_manager_;
  scoped_refptr<ExtensionMessageService> extension_message_service_;
  scoped_ptr<SSLHostState> ssl_host_state_;
  scoped_refptr<net::TransportSecurityState>
      transport_security_state_;
  scoped_refptr<TransportSecurityPersister>
      transport_security_persister_;
  scoped_ptr<PrefService> prefs_;
  scoped_refptr<ThumbnailStore> thumbnail_store_;
  scoped_ptr<TemplateURLFetcher> template_url_fetcher_;
  scoped_ptr<TemplateURLModel> template_url_model_;
  scoped_ptr<BookmarkModel> bookmark_bar_model_;
  scoped_refptr<WebResourceService> web_resource_service_;
  scoped_ptr<NTPResourceCache> ntp_resource_cache_;

  scoped_ptr<ProfileSyncService> sync_service_;

  scoped_refptr<ChromeURLRequestContextGetter> request_context_;

  scoped_refptr<ChromeURLRequestContextGetter> media_request_context_;

  scoped_refptr<ChromeURLRequestContextGetter> extensions_request_context_;

  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_refptr<BlacklistManager> blacklist_manager_;
  scoped_refptr<DownloadManager> download_manager_;
  scoped_refptr<HistoryService> history_service_;
  scoped_refptr<FaviconService> favicon_service_;
  scoped_ptr<SearchVersusNavigateClassifier> search_versus_navigate_classifier_;
  scoped_refptr<WebDataService> web_data_service_;
  scoped_refptr<PasswordStore> password_store_;
  scoped_refptr<SessionService> session_service_;
  scoped_ptr<BrowserThemeProvider> theme_provider_;
  scoped_refptr<WebKitContext> webkit_context_;
  scoped_ptr<DesktopNotificationService> desktop_notification_service_;
  scoped_ptr<PersonalDataManager> personal_data_manager_;
  bool blacklist_manager_created_;
  bool history_service_created_;
  bool favicon_service_created_;
  bool created_web_data_service_;
  bool created_password_store_;
  bool created_download_manager_;
  bool created_theme_provider_;
  // Whether or not the last session exited cleanly. This is set only once.
  bool last_session_exited_cleanly_;

  base::OneShotTimer<ProfileImpl> create_session_service_timer_;

  scoped_ptr<OffTheRecordProfileImpl> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

  scoped_refptr<TabRestoreService> tab_restore_service_;

  scoped_refptr<SpellCheckHost> spellcheck_host_;

  // Indicates whether |spellcheck_host_| has told us initialization is
  // finished.
  bool spellcheck_host_ready_;

  // Set to true when ShutdownSessionService is invoked. If true
  // GetSessionService won't recreate the SessionService.
  bool shutdown_session_service_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

#if defined(OS_CHROMEOS)
  chromeos::Preferences chromeos_preferences_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILE_H_
