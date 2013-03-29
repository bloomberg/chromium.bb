// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_
#define CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/ibus/ibus_component.h"

namespace chromeos {

// Represents a component extension IME.
struct CHROMEOS_EXPORT ComponentExtensionIME {
  ComponentExtensionIME();
  ~ComponentExtensionIME();
  std::string id;  // extension id.
  std::string description;  // description of extension.
  base::FilePath path;
  std::vector<IBusComponent::EngineDescription> engines;
};

// Provides an interface to list/load/unload for component extension IME.
class CHROMEOS_EXPORT ComponentExtensionIMEManagerDelegate {
 public:
  ComponentExtensionIMEManagerDelegate();
  virtual ~ComponentExtensionIMEManagerDelegate();

  // Lists installed component extension IMEs.
  virtual std::vector<ComponentExtensionIME> ListIME() = 0;

  // Loads component extension IME associated with |extension_id|.
  // Returns false if it fails, otherwise returns true.
  virtual bool Load(const std::string& extension_id,
                    const base::FilePath& path) = 0;

  // Unloads component extension IME associated with |extension_id|.
  // Returns false if it fails, otherwise returns true;
  virtual bool Unload(const std::string& extension_id,
                      const base::FilePath& path) = 0;
};

// This class manages component extension input method.
class CHROMEOS_EXPORT ComponentExtensionIMEManager {
 public:
  // This class takes the ownership of |delegate|.
  explicit ComponentExtensionIMEManager(
      ComponentExtensionIMEManagerDelegate* delegate);
  virtual ~ComponentExtensionIMEManager();

  // Initializes component extension manager. This function create internal
  // mapping between input method id and engine components. This function must
  // be called before using any other function.
  void Initialize();

  // Loads |input_method_id| component extension IME. This function returns true
  // on success. This function is safe to call multiple times. Returns false if
  // already corresponding component extension is loaded.
  bool LoadComponentExtensionIME(const std::string& input_method_id);

  // Unloads |input_method_id| component extension IME. This function returns
  // true on success. This function is safe to call multiple times. Returns
  // false if already corresponding component extension is unloaded.
  bool UnloadComponentExtensionIME(const std::string& input_method_id);

  // Returns true if |input_method_id| is component extension ime id.
  bool IsComponentExtensionIMEId(const std::string& input_method_id);

  // Returns localized name of |input_method_id|.
  std::string GetName(const std::string& input_method_id);

  // Returns localized description of |input_method_id|.
  std::string GetDescription(const std::string& input_method_id);

  // Returns list of input method id associated with |language|.
  std::vector<std::string> ListIMEByLanguage(const std::string& language);

 private:
  // Finds ComponentExtensionIME and EngineDescription associated with
  // |input_method_id|. This function retruns true if it is found, otherwise
  // returns false. |out_extension| and |out_engine| can be NULL.
  bool FindEngineEntry(const std::string& input_method_id,
                       ComponentExtensionIME* out_extension,
                       IBusComponent::EngineDescription* out_engine);
  scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate_;

  std::vector<ComponentExtensionIME> component_extension_imes_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_
