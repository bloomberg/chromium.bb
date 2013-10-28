// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_DELEGATE_H_
#define CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/one_shot_event.h"

class TemplateURLService;

namespace base {
class DictionaryValue;
class ListValue;
}

// Defines the interface for the delegate that will interact with the rest of
// the browser on behalf of the AutomaticProfileResetter.
// The primary reason for this separation is to facilitate unit testing.
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

  // Returns a list of loaded module name digests.
  virtual scoped_ptr<base::ListValue> GetLoadedModuleNameDigests() const = 0;

  // Returns attributes of the search engine currently set as the default (or
  // an empty dictionary if there is none).
  // The returned attributes correspond in meaning and format to the user
  // preferences stored by TemplateURLService::SaveDefaultSearchProviderToPrefs,
  // and will be named after the second path name segment of the respective
  // preference (i.e. the part after "default_search_provider.").
  // Note that:
  //  1.) the "enabled" attribute will not be present, as it is not technically
  //      an attribute of a search provider,
  //  2.) "encodings" will be a list of strings, in contrast to a single string
  //      with tokens delimited by semicolons, but the order will be the same.
  virtual scoped_ptr<base::DictionaryValue>
      GetDefaultSearchProviderDetails() const = 0;

  // Returns whether or not the default search provider is set by policy.
  virtual bool IsDefaultSearchProviderManaged() const = 0;

  // Returns a list of dictionaries, each containing attributes for each of the
  // pre-populated search engines, in the format described above.
  virtual scoped_ptr<base::ListValue>
      GetPrepopulatedSearchProvidersDetails() const = 0;

  // Triggers showing the one-time profile settings reset prompt.
  virtual void ShowPrompt() = 0;
};

// Implementation for AutomaticProfileResetterDelegate.
// To facilitate unit testing, having the TemplateURLService available is only
// required when using search engine related methods.
class AutomaticProfileResetterDelegateImpl
    : public AutomaticProfileResetterDelegate,
      public TemplateURLServiceObserver,
      public content::NotificationObserver {
 public:
  // The |template_url_service| may be NULL in unit tests. Must be non-NULL for
  // callers who wish to call:
  //  * LoadTemplateURLServiceIfNeeded(),
  //  * GetDefaultSearchProviderDetails(),
  //  * GetPrepopulatedSearchProvidersDetails(), or
  //  * IsDefaultSearchManaged().
  explicit AutomaticProfileResetterDelegateImpl(
      TemplateURLService* template_url_service);
  virtual ~AutomaticProfileResetterDelegateImpl();

  // AutomaticProfileResetterDelegate:
  virtual void EnumerateLoadedModulesIfNeeded() OVERRIDE;
  virtual void RequestCallbackWhenLoadedModulesAreEnumerated(
      const base::Closure& ready_callback) const OVERRIDE;
  virtual void LoadTemplateURLServiceIfNeeded() OVERRIDE;
  virtual void RequestCallbackWhenTemplateURLServiceIsLoaded(
      const base::Closure& ready_callback) const OVERRIDE;
  virtual scoped_ptr<base::ListValue>
      GetLoadedModuleNameDigests() const OVERRIDE;
  virtual scoped_ptr<base::DictionaryValue>
      GetDefaultSearchProviderDetails() const OVERRIDE;
  virtual bool IsDefaultSearchProviderManaged() const OVERRIDE;
  virtual scoped_ptr<base::ListValue>
      GetPrepopulatedSearchProvidersDetails() const OVERRIDE;
  virtual void ShowPrompt() OVERRIDE;

  // TemplateURLServiceObserver:
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  TemplateURLService* template_url_service_;

  content::NotificationRegistrar registrar_;

  // The list of modules found. Even when |modules_have_been_enumerated_event_|
  // is signaled, this may still be NULL.
  scoped_ptr<base::ListValue> module_list_;

  // This event is signaled once module enumeration has been attempted.  If
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

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateImpl);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_AUTOMATIC_PROFILE_RESETTER_DELEGATE_H_
