// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_

#include <string>
#include <vector>

#include "base/version.h"
#include "googleurl/src/gurl.h"

class FilePath;

namespace net {
class URLRequestContextGetter;
}

namespace base {
class DictionaryValue;
}

// Component specific installers must derive from this class and implement
// OnUpdateError() and Install(). A valid instance of this class must be
// given to ComponentUpdateService::RegisterComponent().
class ComponentInstaller {
 public :
  // Called by the component updater on the UI thread when there was a
  // problem unpacking or verifying the component. |error| is a non-zero
  // value which is only meaninful to the component updater.
  virtual void OnUpdateError(int error) = 0;

  // Called by the component updater when a component has been unpacked
  // and is ready to be installed. |manifest| contains the CRX manifest
  // json dictionary and |unpack_path| contains the temporary directory
  // with all the unpacked CRX files.
  virtual bool Install(base::DictionaryValue* manifest,
                       const FilePath& unpack_path) = 0;

 protected:
  virtual ~ComponentInstaller() {}
};

// Describes a particular component that can be installed or updated. This
// structure is required to register a component with the component updater.
// Only |name| is optional. |pk_hash| is the SHA256 hash of the component's
// public key. If the component is to be installed then version should be
// "0" or "0.0", else it should be the current version.
// |source| is by default pointing to BANDAID but if needed it can be made
// to point to the webstore (CWS_PUBLIC) or to the webstore sandbox. It is
// important to note that the BANDAID source if active throught the day
// can pre-empt updates from the other sources down the list.
struct CrxComponent {
  // Specifies the source url for manifest check.
  enum UrlSource {
    BANDAID,
    CWS_PUBLIC,
    CWS_SANDBOX
  };

  std::vector<uint8> pk_hash;
  ComponentInstaller* installer;
  Version version;
  std::string name;
  UrlSource source;
  CrxComponent();
  ~CrxComponent();
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
    kError
  };
  // Controls the component updater behavior.
  class Configurator {
   public:
    enum Events {
      kManifestCheck,
      kComponentUpdated,
      kManifestError,
      kNetworkError,
      kUnpackError,
      kInstallerError
    };

    virtual ~Configurator() {}
    // Delay in seconds from calling Start() to the first update check.
    virtual int InitialDelay() = 0;
    // Delay in seconds to every subsequent update check. 0 means don't check.
    virtual int NextCheckDelay() = 0;
    // Delay in seconds from each task step. Used to smooth out CPU/IO usage.
    virtual int StepDelay() = 0;
    // Minimun delta time in seconds before checking again the same component.
    virtual int MinimumReCheckWait() = 0;
    // The url that is going to be used update checks over Omaha protocol.
    virtual GURL UpdateUrl(CrxComponent::UrlSource source) = 0;
    // Parameters added to each url request. It can be null if none are needed.
    virtual const char* ExtraRequestParams() = 0;
    // How big each update request can be. Don't go above 2000.
    virtual size_t UrlSizeLimit() = 0;
    // The source of contexts for all the url requests.
    virtual net::URLRequestContextGetter* RequestContext() = 0;
    // True means that all ops are performed in this process.
    virtual bool InProcess() = 0;
    // The component updater will call this function when an interesting event
    // happens. It should be used mostly as a place to add application specific
    // logging or telemetry. |extra| is |event| dependent.
    virtual void OnEvent(Events event, int extra) = 0;
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

  virtual ~ComponentUpdateService() {}
};

// Creates the component updater. You must pass a valid |config| allocated on
// the heap which the component updater will own.
ComponentUpdateService* ComponentUpdateServiceFactory(
    ComponentUpdateService::Configurator* config);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_H_

