// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_

#include <string>
#include <vector>

#include "base/version.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

class ComponentPatcher;

// Component specific installers must derive from this class and implement
// OnUpdateError() and Install(). A valid instance of this class must be
// given to ComponentUpdateService::RegisterComponent().
class ComponentInstaller {
 public :
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

// Defines an interface to observe a CrxComponent.
class ComponentObserver {
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
  };

  virtual ~ComponentObserver() {}

  // The component updater service will call this function when an interesting
  // event happens for a specific component. |extra| is |event| dependent.
  virtual void OnEvent(Events event, int extra) = 0;
};

// Describes a particular component that can be installed or updated. This
// structure is required to register a component with the component updater.
// |pk_hash| is the SHA256 hash of the component's public key. If the component
// is to be installed then version should be "0" or "0.0", else it should be
// the current version. |observer|, |fingerprint|, and |name| are optional.
struct CrxComponent {
  std::vector<uint8> pk_hash;
  ComponentInstaller* installer;
  ComponentObserver* observer;
  Version version;
  std::string fingerprint;
  std::string name;
  CrxComponent();
  ~CrxComponent();
};

// This convenience function returns component id of given CrxComponent.
std::string GetCrxComponentID(const CrxComponent& component);

// Convenience structure to use with component listing / enumeration.
struct CrxComponentInfo {
  // |id| is currently derived from |CrxComponent.pk_hash|, see rest of the
  // class implementation for details.
  std::string id;
  std::string version;
  std::string name;
  CrxComponentInfo();
  ~CrxComponentInfo();
};

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
  enum Status {
    kOk,
    kReplaced,
    kInProgress,
    kError
  };
  // Controls the component updater behavior.
  class Configurator {
   public:
    virtual ~Configurator() {}
    // Delay in seconds from calling Start() to the first update check.
    virtual int InitialDelay() = 0;
    // Delay in seconds to every subsequent update check. 0 means don't check.
    virtual int NextCheckDelay() = 0;
    // Delay in seconds from each task step. Used to smooth out CPU/IO usage.
    virtual int StepDelay() = 0;
    // Delay in seconds between applying updates for different components, if
    // several updates are available at a given time.
    virtual int StepDelayMedium() = 0;
    // Minimum delta time in seconds before checking again the same component.
    virtual int MinimumReCheckWait() = 0;
    // Minimum delta time in seconds before an on-demand check is allowed
    // for the same component.
    virtual int OnDemandDelay() = 0;
    // The url that is going to be used update checks over Omaha protocol.
    virtual GURL UpdateUrl() = 0;
    // The url where the completion pings are sent. Invalid if and only if
    // pings are disabled.
    virtual GURL PingUrl() = 0;
    // Parameters added to each url request. It can be null if none are needed.
    virtual const char* ExtraRequestParams() = 0;
    // How big each update request can be. Don't go above 2000.
    virtual size_t UrlSizeLimit() = 0;
    // The source of contexts for all the url requests.
    virtual net::URLRequestContextGetter* RequestContext() = 0;
    // True means that all ops are performed in this process.
    virtual bool InProcess() = 0;
    // Creates a new ComponentPatcher in a platform-specific way. This is useful
    // for dependency injection.
    virtual ComponentPatcher* CreateComponentPatcher() = 0;
    // True means that this client can handle delta updates.
    virtual bool DeltasEnabled() const = 0;
  };

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
  virtual void GetComponents(std::vector<CrxComponentInfo>* components) = 0;

  virtual ~ComponentUpdateService() {}

  // TODO(waffles): Remove PNaCl as a friend once an alternative on-demand
  // trigger is available.
  friend class ComponentsUI;
  friend class PnaclComponentInstaller;
  friend class OnDemandTester;

 private:
  // Ask the component updater to do an update check for a previously
  // registered component, immediately. If an update or check is already
  // in progress, returns |kInProgress|.
  // There is no guarantee that the item will actually be updated,
  // since an update may not be available. Listeners for the component will
  // know the outcome of the check.
  virtual Status OnDemandUpdate(const std::string& component_id) = 0;
};

// Creates the component updater. You must pass a valid |config| allocated on
// the heap which the component updater will own.
ComponentUpdateService* ComponentUpdateServiceFactory(
    ComponentUpdateService::Configurator* config);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
