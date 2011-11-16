// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/net/preconnect.h" // TODO: remove this.
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/browser/browser_context.h"

namespace base {
class Time;
}

namespace chromeos {
class LibCrosServiceLibraryImpl;
class ResetDefaultProxyConfigServiceTask;
}

namespace fileapi {
class FileSystemContext;
}

namespace history {
class TopSites;
class ShortcutsBackend;
}

namespace net {
class SSLConfigService;
}

namespace chrome_browser_net {
class Predictor;
}

class AutocompleteClassifier;
class BookmarkModel;
class ChromeAppCacheService;
class ChromeURLDataManager;
class Extension;
class ExtensionDevToolsManager;
class ExtensionEventRouter;
class ExtensionInfoMap;
class ExtensionMessageService;
class ExtensionProcessManager;
class ExtensionService;
class ExtensionSpecialStoragePolicy;
class FaviconService;
class FindBarState;
class HistoryService;
class HostContentSettingsMap;
class NetworkActionPredictor;
class PasswordStore;
class PrefService;
class ProfileSyncService;
class PromoCounter;
class ProtocolHandlerRegistry;
class SpeechInputPreferences;
class SpellCheckHost;
class TemplateURLFetcher;
class TestingProfile;
class TokenService;
class UserScriptMaster;
class UserStyleSheetWatcher;
class VisitedLinkMaster;
class WebDataService;
class WebUI;

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// Local profile ids are used to associate resources stored outside the profile
// directory, like saved passwords in GNOME Keyring / KWallet, with a profile.
// With high probability, they are unique on the local machine. They are almost
// certainly not unique globally, by design. Do not send them over the network.
typedef int LocalProfileId;
#endif

class Profile : public content::BrowserContext {
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
    // performed while incognito. (ex: creating a bookmark)
    //
    // Since EXPLICIT_ACCESS means "as a result of a user action", this request
    // always succeeds.
    EXPLICIT_ACCESS,

    // The caller plans to call a method that will permanently change some data
    // in the profile, as part of Chrome's implicit data logging. Use this flag
    // when you are about to perform an operation which is incompatible with the
    // incognito mode.
    IMPLICIT_ACCESS
  };

  class Delegate {
   public:
    // Called when creation of the profile is finished.
    virtual void OnProfileCreated(Profile* profile, bool success) = 0;
  };

  // Whitelist access to deprecated API in order to prevent new regressions.
  class Deprecated {
   private:
    friend bool IsGoogleGAIACookieInstalled();
    friend void chrome_browser_net::PreconnectOnIOThread(
        const GURL&,
        chrome_browser_net::UrlInfo::ResolutionMotivation,
        int);

    friend class AutofillDownloadManager;
    friend class ChromePluginMessageFilter;
    friend class BrowserListTabContentsProvider;
    friend class MetricsService;
    friend class SafeBrowsingServiceTestHelper;
    friend class SdchDictionaryFetcher;
    friend class SyncTest;
    friend class Toolbar5Importer;
    friend class TranslateManager;
    friend class chromeos::LibCrosServiceLibraryImpl;
    friend class chromeos::ResetDefaultProxyConfigServiceTask;

    static net::URLRequestContextGetter* GetDefaultRequestContext() {
      return Profile::GetDefaultRequestContext();
    }
  };

  // Key used to bind profile to the widget with which it is associated.
  static const char* const kProfileKey;

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
  // Value that represents no local profile id.
  static const LocalProfileId kInvalidLocalProfileId;
#endif

  Profile();
  virtual ~Profile() {}

  // Profile prefs are registered as soon as the prefs are loaded for the first
  // time.
  static void RegisterUserPrefs(PrefService* prefs);

  // Create a new profile given a path.
  static Profile* CreateProfile(const FilePath& path);

  // Same as above, but uses async initialization.
  static Profile* CreateProfileAsync(const FilePath& path,
                                     Delegate* delegate);

  // Returns the profile corresponding to the given browser context.
  static Profile* FromBrowserContext(content::BrowserContext* browser_context);

  // Returns the profile corresponding to the given WebUI.
  static Profile* FromWebUI(WebUI* web_ui);

  // content::BrowserContext implementation ------------------------------------

  virtual FilePath GetPath() = 0;
  virtual SSLHostState* GetSSLHostState() = 0;
  virtual DownloadManager* GetDownloadManager() = 0;
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) = 0;
  virtual net::URLRequestContextGetter* GetRequestContextForMedia() = 0;
  virtual const content::ResourceContext& GetResourceContext() = 0;
  virtual HostZoomMap* GetHostZoomMap() = 0;
  virtual GeolocationPermissionContext* GetGeolocationPermissionContext() = 0;
  virtual SpeechInputPreferences* GetSpeechInputPreferences() = 0;
  virtual quota::QuotaManager* GetQuotaManager() = 0;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() = 0;
  virtual WebKitContext* GetWebKitContext() = 0;
  virtual ChromeAppCacheService* GetAppCacheService() = 0;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() = 0;
  virtual fileapi::FileSystemContext* GetFileSystemContext() = 0;

  // content::BrowserContext implementation ------------------------------------

  // Typesafe upcast.
  virtual TestingProfile* AsTestingProfile();

  // Returns the name associated with this profile. This name is displayed in
  // the browser frame.
  virtual std::string GetProfileName() = 0;

  // Return whether this profile is incognito. Default is false.
  virtual bool IsOffTheRecord() = 0;

  // Return the incognito version of this profile. The returned pointer
  // is owned by the receiving profile. If the receiving profile is off the
  // record, the same profile is returned.
  //
  // WARNING: This will create the OffTheRecord profile if it doesn't already
  // exist. If this isn't what you want, you need to check
  // HasOffTheRecordProfile() first.
  virtual Profile* GetOffTheRecordProfile() = 0;

  // Destroys the incognito profile.
  virtual void DestroyOffTheRecordProfile() = 0;

  // True if an incognito profile exists.
  virtual bool HasOffTheRecordProfile() = 0;

  // Return the original "recording" profile. This method returns this if the
  // profile is not incognito.
  virtual Profile* GetOriginalProfile() = 0;

  // Returns a pointer to the TopSites (thumbnail manager) instance
  // for this profile.
  virtual history::TopSites* GetTopSites() = 0;

  // Variant of GetTopSites that doesn't force creation.
  virtual history::TopSites* GetTopSitesWithoutCreating() = 0;

  // Retrieves a pointer to the VisitedLinkMaster associated with this
  // profile.  The VisitedLinkMaster is lazily created the first time
  // that this method is called.
  virtual VisitedLinkMaster* GetVisitedLinkMaster() = 0;

  // Retrieves a pointer to the ExtensionService associated with this
  // profile. The ExtensionService is created at startup.
  virtual ExtensionService* GetExtensionService() = 0;

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

  // Accessor. The instance is created at startup.
  virtual ExtensionEventRouter* GetExtensionEventRouter() = 0;

  // Accessor. The instance is created upon first access.
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() = 0;

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

  // Retrieves a pointer to the AutocompleteClassifier associated with this
  // profile. The AutocompleteClassifier is lazily created the first time that
  // this method is called.
  virtual AutocompleteClassifier* GetAutocompleteClassifier() = 0;

  // Returns the ShortcutsBackend for this profile. This is owned by
  // the Profile and created on the first call. Callers that outlive the life of
  // this profile need to be sure they refcount the returned value.
  virtual history::ShortcutsBackend* GetShortcutsBackend() = 0;

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
  // This may return NULL if the implementation is unable to create a
  // password store (e.g. a corrupt database).
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access) = 0;

  // Retrieves a pointer to the PrefService that manages the preferences
  // for this user profile.  The PrefService is lazily created the first
  // time that this method is called.
  virtual PrefService* GetPrefs() = 0;

  // Retrieves a pointer to the PrefService that manages the preferences
  // for OffTheRecord Profiles.  This PrefService is lazily created the first
  // time that this method is called.
  virtual PrefService* GetOffTheRecordPrefs() = 0;

  // Returns the TemplateURLFetcher for this profile. This is owned by the
  // profile.
  virtual TemplateURLFetcher* GetTemplateURLFetcher() = 0;

  // Returns the request context used for extension-related requests.  This
  // is only used for a separate cookie store currently.
  virtual net::URLRequestContextGetter* GetRequestContextForExtensions() = 0;

  // Returns the request context used within an installed app that has
  // requested isolated storage.
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id) = 0;

  // Called by the ExtensionService that lives in this profile. Gives the
  // profile a chance to react to the load event before the EXTENSION_LOADED
  // notification has fired. The purpose for handling this event first is to
  // avoid race conditions by making sure URLRequestContexts learn about new
  // extensions before anything else needs them to know.
  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) {}

  // Called by the ExtensionService that lives in this profile. Lets the
  // profile clean up its RequestContexts once all the listeners to the
  // EXTENSION_UNLOADED notification have finished running.
  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const extension_misc::UnloadedExtensionReason) {}

  // Returns the SSLConfigService for this profile.
  virtual net::SSLConfigService* GetSSLConfigService() = 0;

  // Returns the Hostname <-> Content settings map for this profile.
  virtual HostContentSettingsMap* GetHostContentSettingsMap() = 0;

  // Returns the user style sheet watcher.
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher() = 0;

  // Returns the find bar state for this profile.  The find bar state is lazily
  // created the first time that this method is called.
  virtual FindBarState* GetFindBarState() = 0;

  // Returns true if this profile has a profile sync service.
  virtual bool HasProfileSyncService() const = 0;

  // Returns true if the last time this profile was open it was exited cleanly.
  virtual bool DidLastSessionExitCleanly() = 0;

  // Returns the BookmarkModel, creating if not yet created.
  virtual BookmarkModel* GetBookmarkModel() = 0;

  // Returns the ProtocolHandlerRegistry, creating if not yet created.
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() = 0;

  // Returns the Gaia Token Service, creating if not yet created.
  virtual TokenService* GetTokenService() = 0;

  // Returns the ProfileSyncService, creating if not yet created.
  virtual ProfileSyncService* GetProfileSyncService() = 0;

  // Returns the ProfileSyncService, creating if not yet created, with
  // the specified CrOS username.
  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_user) = 0;

  // Return whether 2 profiles are the same. 2 profiles are the same if they
  // represent the same profile. This can happen if there is pointer equality
  // or if one profile is the incognito version of another profile (or vice
  // versa).
  virtual bool IsSameProfile(Profile* profile) = 0;

  // Returns the time the profile was started. This is not the time the profile
  // was created, rather it is the time the user started chrome and logged into
  // this profile. For the single profile case, this corresponds to the time
  // the user started chrome.
  virtual base::Time GetStartTime() const = 0;

  // May return NULL.
  virtual SpellCheckHost* GetSpellCheckHost() = 0;

  // If |force| is false, and the spellchecker is already initialized (or is in
  // the process of initializing), then do nothing. Otherwise clobber the
  // current spellchecker and replace it with a new one.
  virtual void ReinitializeSpellCheckHost(bool force) = 0;

  // Marks the profile as cleanly shutdown.
  //
  // NOTE: this is invoked internally on a normal shutdown, but is public so
  // that it can be invoked when the user logs out/powers down (WM_ENDSESSION).
  virtual void MarkAsCleanShutdown() = 0;

  // Initializes extensions machinery.
  // Component extensions are always enabled, external and user extensions
  // are controlled by |extensions_enabled|.
  virtual void InitExtensions(bool extensions_enabled) = 0;

  // Start up service that gathers data from a promo resource feed.
  virtual void InitPromoResources() = 0;

  // Register URLRequestFactories for protocols registered with
  // registerProtocolHandler.
  virtual void InitRegisteredProtocolHandlers() = 0;

  // Returns the last directory that was chosen for uploading or opening a file.
  virtual FilePath last_selected_directory() = 0;
  virtual void set_last_selected_directory(const FilePath& path) = 0;

  // Returns the IO-thread-accessible profile data for this profile.
  virtual ExtensionInfoMap* GetExtensionInfoMap() = 0;

  // Returns the PromoCounter for Instant, or NULL if not applicable.
  virtual PromoCounter* GetInstantPromoCounter() = 0;

  // Returns the ChromeURLDataManager for this profile.
  virtual ChromeURLDataManager* GetChromeURLDataManager() = 0;

#if defined(OS_CHROMEOS)
  enum AppLocaleChangedVia {
    // Caused by chrome://settings change.
    APP_LOCALE_CHANGED_VIA_SETTINGS,
    // Locale has been reverted via LocaleChangeGuard.
    APP_LOCALE_CHANGED_VIA_REVERT,
    // From login screen.
    APP_LOCALE_CHANGED_VIA_LOGIN,
    // Source unknown.
    APP_LOCALE_CHANGED_VIA_UNKNOWN
  };

  // Changes application locale for a profile.
  virtual void ChangeAppLocale(
      const std::string& locale, AppLocaleChangedVia via) = 0;

  // Called after login.
  virtual void OnLogin() = 0;

  // Creates ChromeOS's EnterpriseExtensionListener.
  virtual void SetupChromeOSEnterpriseExtensionObserver() = 0;

  // Initializes Chrome OS's preferences.
  virtual void InitChromeOSPreferences() = 0;
#endif  // defined(OS_CHROMEOS)

  // Returns the helper object that provides the proxy configuration service
  // access to the the proxy configuration possibly defined by preferences.
  virtual PrefProxyConfigTracker* GetProxyConfigTracker() = 0;

  // Returns the Predictor object used for dns prefetch.
  virtual chrome_browser_net::Predictor* GetNetworkPredictor() = 0;

  // Deletes all network related data since |time|. It deletes transport
  // security state since |time| and it also delete HttpServerProperties data.
  // The implementation is free to run this on a background thread, so when this
  // method returns data is not guaranteed to be deleted.
  virtual void ClearNetworkingHistorySince(base::Time time) = 0;

  // Returns the home page for this profile.
  virtual GURL GetHomePage() = 0;

  // Returns the NetworkActionPredictor used by the Omnibox to decide when to
  // prerender or prefetch a result.
  virtual NetworkActionPredictor* GetNetworkActionPredictor() = 0;

  std::string GetDebugName();

  // Returns whether it is a guest session.
  static bool IsGuestSession();

#ifdef UNIT_TEST
  // Use with caution.  GetDefaultRequestContext may be called on any thread!
  static void set_default_request_context(net::URLRequestContextGetter* c) {
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

  // Stop sending accessibility events until ResumeAccessibilityEvents().
  // Calls to Pause nest; no events will be sent until the number of
  // Resume calls matches the number of Pause calls received.
  void PauseAccessibilityEvents() {
    accessibility_pause_level_++;
  }

  void ResumeAccessibilityEvents() {
    DCHECK(accessibility_pause_level_ > 0);
    accessibility_pause_level_--;
  }

  bool ShouldSendAccessibilityEvents() {
    return 0 == accessibility_pause_level_;
  }

  // Checks whether sync is configurable by the user. Returns false if sync is
  // disabled or controlled by configuration management.
  bool IsSyncAccessible();

  // Creates an OffTheRecordProfile which points to this Profile.
  Profile* CreateOffTheRecordProfile();

 protected:
  friend class OffTheRecordProfileImpl;

  static net::URLRequestContextGetter* default_request_context_;

 private:
  // ***DEPRECATED**: You should be passing in the specific profile's
  // URLRequestContextGetter or using the system URLRequestContextGetter.
  //
  // Returns the request context for the "default" profile.  This may be called
  // from any thread.  This CAN return NULL if a first request context has not
  // yet been created.  If necessary, listen on the UI thread for
  // NOTIFY_DEFAULT_REQUEST_CONTEXT_AVAILABLE.
  static net::URLRequestContextGetter* GetDefaultRequestContext();

  bool restored_last_session_;

  // Accessibility events will only be propagated when the pause
  // level is zero.  PauseAccessibilityEvents and ResumeAccessibilityEvents
  // increment and decrement the level, respectively, rather than set it to
  // true or false, so that calls can be nested.
  int accessibility_pause_level_;
};

#if defined(COMPILER_GCC)
namespace __gnu_cxx {

template<>
struct hash<Profile*> {
  std::size_t operator()(Profile* const& p) const {
    return reinterpret_cast<std::size_t>(p);
  }
};

}  // namespace __gnu_cxx
#endif

#endif  // CHROME_BROWSER_PROFILES_PROFILE_H_
