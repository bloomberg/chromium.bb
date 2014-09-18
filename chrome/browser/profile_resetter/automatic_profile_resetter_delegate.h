// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declares a delegate that interacts with the rest of the browser on behalf of
// the AutomaticProfileResetter.
//
// The reason for this separation is to facilitate unit testing. Factoring out
// the implementation for each interaction step (encapsulated by one method of
// the delegate) allows it to be tested independently in itself. It also becomes
// easier to verify that the state machine inside AutomaticProfileResetter works
// correctly: by mocking out the interaction methods in the delegate, we can, in
// effect, mock out the entire rest of the browser, allowing us to easily
// simulate scenarios that are interesting for testing the state machine.
//
// The delegate is normally instantiated by AutomaticProfileResetter internally,
// while a mock implementation can be injected during unit tests.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_DELEGATE_H_
#define CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/one_shot_event.h"

class BrandcodeConfigFetcher;
class GlobalErrorService;
class Profile;
class ResettableSettingsSnapshot;
class TemplateURLService;

namespace base {
class DictionaryValue;
class ListValue;
}

// Defines the interface for the delegate that will interact with the rest of
// the browser on behalf of the AutomaticProfileResetter.
class AutomaticProfileResetterDelegate {
 public:
  virtual ~AutomaticProfileResetterDelegate() {}

  // Requests the module enumerator to start scanning for loaded modules now, if
  // it has not done so already.
  virtual void EnumerateLoadedModulesIfNeeded() = 0;

  // Requests |ready_callback| to be posted on the UI thread once the module
  // enumerator has finished scanning for loaded modules.
  virtual void RequestCallbackWhenLoadedModulesAreEnumerated(
      const base::Closure& ready_callback) const = 0;

  // Requests the template URL service to load its database (asynchronously).
  virtual void LoadTemplateURLServiceIfNeeded() = 0;

  // Requests |ready_callback| to be posted on the UI thread once the template
  // URL service has finished loading its database.
  virtual void RequestCallbackWhenTemplateURLServiceIsLoaded(
      const base::Closure& ready_callback) const = 0;

  // Starts downloading the configuration file that specifies the default
  // settings for the current brandcode; or establishes that we are running a
  // vanilla (non-branded) build.
  virtual void FetchBrandcodedDefaultSettingsIfNeeded() = 0;

  // Requests |ready_callback| to be posted on the UI thread once the brandcoded
  // default settings have been downloaded, or once it has been established that
  // we are running a vanilla (non-branded) build.
  virtual void RequestCallbackWhenBrandcodedDefaultsAreFetched(
      const base::Closure& ready_callback) const = 0;

  // Returns a list of loaded module name digests.
  virtual scoped_ptr<base::ListValue> GetLoadedModuleNameDigests() const = 0;

  // Returns attributes of the search engine currently set as the default (or
  // an empty dictionary if there is none).
  // The dictionary is in the same format as persisted to preferences by
  // DefaultSearchManager::SetUserSelectedDefaultSearchEngine.
  virtual scoped_ptr<base::DictionaryValue>
      GetDefaultSearchProviderDetails() const = 0;

  // Returns whether or not the default search provider is set by policy.
  virtual bool IsDefaultSearchProviderManaged() const = 0;

  // Returns a list of dictionaries, each containing attributes for each of the
  // pre-populated search engines, in the format described above.
  virtual scoped_ptr<base::ListValue>
      GetPrepopulatedSearchProvidersDetails() const = 0;

  // Triggers showing the one-time profile settings reset prompt, which consists
  // of two parts: (1.) the profile reset (pop-up) bubble, and (2.) a menu item
  // in the wrench menu. Showing the bubble might be delayed if there is another
  // bubble currently shown, in which case the bubble will be shown the first
  // time a new browser window is opened.
  // Returns true unless the reset prompt is not supported on the current
  // platform, in which case it returns false and does nothing.
  virtual bool TriggerPrompt() = 0;

  // Triggers the ProfileResetter to reset all supported settings and optionally
  // |send_feedback|. Will post |completion| on the UI thread once completed.
  // Brandcoded default settings will be fetched if they are not yet available,
  // the reset will be performed once the download is complete.
  // NOTE: Should only be called at most once during the lifetime of the object.
  virtual void TriggerProfileSettingsReset(bool send_feedback,
                                           const base::Closure& completion) = 0;

  // Dismisses the one-time profile settings reset prompt, if it is currently
  // triggered. Will synchronously destroy the corresponding GlobalError.
  virtual void DismissPrompt() = 0;
};

// Implementation for AutomaticProfileResetterDelegate.
class AutomaticProfileResetterDelegateImpl
    : public AutomaticProfileResetterDelegate,
      public base::SupportsWeakPtr<AutomaticProfileResetterDelegateImpl>,
      public TemplateURLServiceObserver,
      public content::NotificationObserver {
 public:
  explicit AutomaticProfileResetterDelegateImpl(
      Profile* profile,
      ProfileResetter::ResettableFlags resettable_aspects);
  virtual ~AutomaticProfileResetterDelegateImpl();

  // Returns the brandcoded default settings; empty defaults if this is not a
  // branded build; or NULL if FetchBrandcodedDefaultSettingsIfNeeded() has not
  // yet been called or not yet finished. Exposed only for unit tests.
  const BrandcodedDefaultSettings* brandcoded_defaults() const {
    return brandcoded_defaults_.get();
  }

  // AutomaticProfileResetterDelegate:
  virtual void EnumerateLoadedModulesIfNeeded() OVERRIDE;
  virtual void RequestCallbackWhenLoadedModulesAreEnumerated(
      const base::Closure& ready_callback) const OVERRIDE;
  virtual void LoadTemplateURLServiceIfNeeded() OVERRIDE;
  virtual void RequestCallbackWhenTemplateURLServiceIsLoaded(
      const base::Closure& ready_callback) const OVERRIDE;
  virtual void FetchBrandcodedDefaultSettingsIfNeeded() OVERRIDE;
  virtual void RequestCallbackWhenBrandcodedDefaultsAreFetched(
      const base::Closure& ready_callback) const OVERRIDE;
  virtual scoped_ptr<base::ListValue>
      GetLoadedModuleNameDigests() const OVERRIDE;
  virtual scoped_ptr<base::DictionaryValue>
      GetDefaultSearchProviderDetails() const OVERRIDE;
  virtual bool IsDefaultSearchProviderManaged() const OVERRIDE;
  virtual scoped_ptr<base::ListValue>
      GetPrepopulatedSearchProvidersDetails() const OVERRIDE;
  virtual bool TriggerPrompt() OVERRIDE;
  virtual void TriggerProfileSettingsReset(
      bool send_feedback,
      const base::Closure& completion) OVERRIDE;
  virtual void DismissPrompt() OVERRIDE;

  // TemplateURLServiceObserver:
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Sends a feedback |report|, where |report| is supposed to be result of
  // ::SerializeSettingsReport(). Virtual, so it can be mocked out for tests.
  virtual void SendFeedback(const std::string& report) const;

  // Triggers the ProfileResetter to reset |resettable_aspects_| and optionally
  // |send_feedback|. Will invoke |completion| on the UI thread once completed
  // The |brandcoded_defaults_| must already be initialized when this is called.
  void RunProfileSettingsReset(bool send_feedback,
                               const base::Closure& completion);

  // Called by |brandcoded_config_fetcher_| when it finishes downloading the
  // brandcoded default settings (or the download times out).
  void OnBrandcodedDefaultsFetched();

  // Called back by the ProfileResetter once resetting the profile settings has
  // been completed. If |old_settings_snapshot| is non-NULL, will compare it to
  // the new settings and send the differences to Google for analysis. Finally,
  // will post |user_callback|.
  void OnProfileSettingsResetCompleted(
      const base::Closure& user_callback,
      scoped_ptr<ResettableSettingsSnapshot> old_settings_snapshot);

  // The profile that this delegate operates on.
  Profile* profile_;

  // Shortcuts to |profile_| keyed services, to reduce boilerplate. These may be
  // NULL in unit tests that do not initialize the respective service(s).
  GlobalErrorService* global_error_service_;
  TemplateURLService* template_url_service_;

  // Helper to asynchronously download the default settings for the current
  // distribution channel (identified by brand code). Instantiated on-demand.
  scoped_ptr<BrandcodeConfigFetcher> brandcoded_config_fetcher_;

  // Once |brandcoded_defaults_fetched_event_| has fired, this will contain the
  // brandcoded default settings, or empty settings for a non-branded build.
  scoped_ptr<BrandcodedDefaultSettings> brandcoded_defaults_;

  // Overwritten to avoid resetting aspects that will not work in unit tests.
  const ProfileResetter::ResettableFlags resettable_aspects_;

  // The profile resetter to perform the actual reset. Instantiated on-demand.
  scoped_ptr<ProfileResetter> profile_resetter_;

  // Manages registrations/unregistrations for notifications.
  content::NotificationRegistrar registrar_;

  // The list of modules found. Even when |modules_have_been_enumerated_event_|
  // is signaled, this may still be NULL.
  scoped_ptr<base::ListValue> module_list_;

  // This event is signaled once module enumeration has been attempted. If
  // during construction, EnumerateModulesModel can supply a non-empty list of
  // modules, module enumeration has clearly already happened, so the event will
  // be signaled immediately; otherwise, when EnumerateLoadedModulesIfNeeded()
  // is called, it will ask the model to scan the modules, and then signal the
  // event once this process is completed.
  extensions::OneShotEvent modules_have_been_enumerated_event_;

  // This event is signaled once the TemplateURLService has loaded.  If the
  // TemplateURLService was already loaded prior to the creation of this class,
  // the event will be signaled during construction.
  extensions::OneShotEvent template_url_service_ready_event_;

  // This event is signaled once brandcoded default settings have been fetched,
  // or once it has been established that this is not a branded build.
  extensions::OneShotEvent brandcoded_defaults_fetched_event_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateImpl);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_DELEGATE_H_
