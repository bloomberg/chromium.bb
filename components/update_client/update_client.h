// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/version.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace update_client {

// Component specific installers must derive from this class and implement
// OnUpdateError() and Install(). A valid instance of this class must be
// given to ComponentUpdateService::RegisterComponent().
class ComponentInstaller
    : public base::RefCountedThreadSafe<ComponentInstaller> {
 public:
  // Called by the component updater on the main thread when there was a
  // problem unpacking or verifying the component. |error| is a non-zero
  // value which is only meaningful to the component updater.
  virtual void OnUpdateError(int error) = 0;

  // Called by the component updater when a component has been unpacked
  // and is ready to be installed. |manifest| contains the CRX manifest
  // json dictionary and |unpack_path| contains the temporary directory
  // with all the unpacked CRX files. This method may be called from
  // a thread other than the main thread.
  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) = 0;

  // Set |installed_file| to the full path to the installed |file|. |file| is
  // the filename of the file in this component's CRX. Returns false if this is
  // not possible (the file has been removed or modified, or its current
  // location is unknown). Otherwise, returns true.
  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) = 0;

  // Called by the component updater when a component has been unregistered and
  // all versions should be uninstalled from disk. Returns true if
  // uninstallation is supported, false otherwise.
  virtual bool Uninstall() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ComponentInstaller>;

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
  std::vector<uint8_t> pk_hash;
  scoped_refptr<ComponentInstaller> installer;
  Version version;
  std::string fingerprint;
  std::string name;
  bool allow_background_download;
  CrxComponent();
  ~CrxComponent();
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_
