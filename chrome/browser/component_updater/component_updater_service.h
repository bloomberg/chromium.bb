// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/version.h"
#include "url/gurl.h"

class ComponentsUI;

namespace base {
class DictionaryValue;
class FilePath;
}

namespace net {
class URLRequestContextGetter;
class URLRequest;
}

namespace content {
class ResourceThrottle;
}

namespace component_updater {

class Configurator;
class OnDemandUpdater;

// Component specific installers must derive from this class and implement
// OnUpdateError() and Install(). A valid instance of this class must be
// given to ComponentUpdateService::RegisterComponent().
class ComponentInstaller {
 public:
  // Called by the component updater on the UI thread when there was a
  // problem unpacking or verifying the component. |error| is a non-zero
  // value which is only meaningful to the component updater.
  virtual void OnUpdateError(int error) = 0;

  // Called by the component updater when a component has been unpacked
  // and is ready to be installed. |manifest| contains the CRX manifest
  // json dictionary and |unpack_path| contains the temporary directory
  // with all the unpacked CRX files.
  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) = 0;

  // Set |installed_file| to the full path to the installed |file|. |file| is
  // the filename of the file in this component's CRX. Returns false if this is
  // not possible (the file has been removed or modified, or its current
  // location is unknown). Otherwise, returns true.
  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) = 0;

  virtual ~ComponentInstaller() {}
};

// Describes a particular component that can be installed or updated. This
// structure is required to register a component with the component updater.
// |pk_hash| is the SHA256 hash of the component's public key. If the component
// is to be installed then version should be "0" or "0.0", else it should be
// the current version. |fingerprint|, and |name| are optional.
// |allow_background_download| specifies that the component can be background
// downloaded in some cases. The default for this value is |true| and the value
// can be overriden at the registration time. This is a temporary change until
// the issue 340448 is resolved.
struct CrxComponent {
  std::vector<uint8> pk_hash;
  ComponentInstaller* installer;
  Version version;
  std::string fingerprint;
  std::string name;
  bool allow_background_download;
  CrxComponent();
  ~CrxComponent();
};

struct CrxUpdateItem;

// The component update service is in charge of installing or upgrading
// select parts of chrome. Each part is called a component and managed by
// instances of CrxComponent registered using RegisterComponent(). On the
// server, each component is packaged as a CRX which is the same format used
// to package extensions. To the update service each component is identified
// by its public key hash (CrxComponent::pk_hash). If there is an update
// available and its version is bigger than (CrxComponent::version), it will
// be downloaded, verified and unpacked. Then component-specific installer
// ComponentInstaller::Install (of CrxComponent::installer) will be called.
//
// During the normal operation of the component updater some specific
// notifications are fired, like COMPONENT_UPDATER_STARTED and
// COMPONENT_UPDATE_FOUND. See notification_type.h for more details.
//
// All methods are safe to call ONLY from chrome's UI thread.
class ComponentUpdateService {
 public:
  enum Status { kOk, kReplaced, kInProgress, kError };

  // Defines an interface to observe ComponentUpdateService. It provides
  // notifications when state changes occur for the service or for the
  // registered components.
  class Observer {
   public:
    enum Events {
      // Sent when the component updater starts doing update checks.
      COMPONENT_UPDATER_STARTED,

      // Sent when the component updater is going to take a long nap.
      COMPONENT_UPDATER_SLEEPING,

      // Sent when there is a new version of a registered component. After
      // the notification is sent the component will be downloaded.
      COMPONENT_UPDATE_FOUND,

      // Sent when the new component has been downloaded and an installation
      // or upgrade is about to be attempted.
      COMPONENT_UPDATE_READY,

      // Sent when a component has been successfully updated.
      COMPONENT_UPDATED,

      // Sent when a component has not been updated following an update check:
      // either there was no update available, or an update failed.
      COMPONENT_NOT_UPDATED,

      // Sent when component bytes are being downloaded.
      COMPONENT_UPDATE_DOWNLOADING,
    };

    virtual ~Observer() {}

    // The component updater service will call this function when an interesting
    // state change happens. If the |id| is specified, then the event is fired
    // on behalf of a specific component. The implementors of this interface are
    // expected to filter the relevant events based on the component id.
    virtual void OnEvent(Events event, const std::string& id) = 0;
  };

  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer. It is safe for an observer to be removed while
  // the observers are being notified.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Start doing update checks and installing new versions of registered
  // components after Configurator::InitialDelay() seconds.
  virtual Status Start() = 0;

  // Stop doing update checks. In-flight requests and pending installations
  // will not be canceled.
  virtual Status Stop() = 0;

  // Add component to be checked for updates. You can call this method
  // before calling Start().
  virtual Status RegisterComponent(const CrxComponent& component) = 0;

  // Returns a list of registered components.
  virtual std::vector<std::string> GetComponentIDs() const = 0;

  // Returns an interface for on-demand updates. On-demand updates are
  // proactively triggered outside the normal component update service schedule.
  virtual OnDemandUpdater& GetOnDemandUpdater() = 0;

  virtual ~ComponentUpdateService() {}

 private:
  // Returns details about registered component in the |item| parameter. The
  // function returns true in case of success and false in case of errors.
  virtual bool GetComponentDetails(const std::string& component_id,
                                   CrxUpdateItem* item) const = 0;

  friend class ::ComponentsUI;
  FRIEND_TEST_ALL_PREFIXES(ComponentUpdaterTest, ResourceThrottleLiveNoUpdate);
};

typedef ComponentUpdateService::Observer ServiceObserver;

class OnDemandUpdater {
 public:
  virtual ~OnDemandUpdater() {}

  // Returns a network resource throttle. It means that a component will be
  // downloaded and installed before the resource is unthrottled. This function
  // can be called from the IO thread. The function implements a cooldown
  // interval of 30 minutes. That means it will ineffective to call the
  // function before the cooldown interval has passed. This behavior is intended
  // to be defensive against programming bugs, usually triggered by web fetches,
  // where the on-demand functionality is invoked too often.
  virtual content::ResourceThrottle* GetOnDemandResourceThrottle(
      net::URLRequest* request,
      const std::string& crx_id) = 0;

 private:
  friend class OnDemandTester;
  friend class ::ComponentsUI;

  // Triggers an update check for a component. |component_id| is a value
  // returned by GetCrxComponentID(). If an update for this component is already
  // in progress, the function returns |kInProgress|. If an update is available,
  // the update will be applied. The caller can subscribe to component update
  // service notifications to get an indication about the outcome of the
  // on-demand update. The function does not implement any cooldown interval.
  virtual ComponentUpdateService::Status OnDemandUpdate(
      const std::string& component_id) = 0;
};

// Creates the component updater. You must pass a valid |config| allocated on
// the heap which the component updater will own.
ComponentUpdateService* ComponentUpdateServiceFactory(Configurator* config);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
