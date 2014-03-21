// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_
#define CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_

#include <set>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/ime/input_method_descriptor.h"

namespace chromeos {

// Represents an engine in component extension IME.
struct CHROMEOS_EXPORT ComponentExtensionEngine {
  ComponentExtensionEngine();
  ~ComponentExtensionEngine();
  std::string engine_id;  // The engine id.
  std::string display_name;  // The display name.
  std::vector<std::string> language_codes;  // The engine's language(ex. "en").
  std::string description;  // The engine description.
  std::vector<std::string> layouts;  // The list of keyboard layout of engine.
  GURL options_page_url; // an URL to option page.
  GURL input_view_url; // an URL to input view page.
};

// Represents a component extension IME.
struct CHROMEOS_EXPORT ComponentExtensionIME {
  ComponentExtensionIME();
  ~ComponentExtensionIME();
  std::string id;  // extension id.
  std::string manifest;  // the contents of manifest.json
  std::string description;  // description of extension.
  GURL options_page_url; // an URL to option page.
  base::FilePath path;
  std::vector<ComponentExtensionEngine> engines;
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
                    const std::string& manifest,
                    const base::FilePath& path) = 0;

  // Unloads component extension IME associated with |extension_id|.
  virtual void Unload(const std::string& extension_id,
                      const base::FilePath& path) = 0;
};

// This class manages component extension input method.
class CHROMEOS_EXPORT ComponentExtensionIMEManager {
 public:
  class Observer {
   public:
    // Called when the initialization is done.
    virtual void OnImeComponentExtensionInitialized() = 0;
  };

  ComponentExtensionIMEManager();
  virtual ~ComponentExtensionIMEManager();

  // Initializes component extension manager. This function create internal
  // mapping between input method id and engine components. This function must
  // be called before using any other function.
  void Initialize(scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate);

  // Notifies the observers for the component extension IMEs are initialized.
  void NotifyInitialized();

  // Returns true if the initialization is done, otherwise returns false.
  bool IsInitialized();

  // Loads |input_method_id| component extension IME. This function returns true
  // on success. This function is safe to call multiple times. Returns false if
  // already corresponding component extension is loaded.
  bool LoadComponentExtensionIME(const std::string& input_method_id);

  // Unloads |input_method_id| component extension IME. This function returns
  // true on success. This function is safe to call multiple times. Returns
  // false if already corresponding component extension is unloaded.
  bool UnloadComponentExtensionIME(const std::string& input_method_id);

  // Returns true if |input_method_id| is whitelisted component extension input
  // method.
  bool IsWhitelisted(const std::string& input_method_id);

  // Returns true if |extension_id| is whitelisted component extension.
  bool IsWhitelistedExtension(const std::string& extension_id);

  // Returns InputMethodId. This function returns empty string if |extension_id|
  // and |engine_id| is not a whitelisted component extention IME.
  std::string GetId(const std::string& extension_id,
                    const std::string& engine_id);

  // Returns localized name of |input_method_id|.
  std::string GetName(const std::string& input_method_id);

  // Returns localized description of |input_method_id|.
  std::string GetDescription(const std::string& input_method_id);

  // Returns list of input method id associated with |language|.
  std::vector<std::string> ListIMEByLanguage(const std::string& language);

  // Returns all IME as InputMethodDescriptors.
  input_method::InputMethodDescriptors GetAllIMEAsInputMethodDescriptor();

  // Returns all XKB keyboard IME as InputMethodDescriptors.
  input_method::InputMethodDescriptors GetXkbIMEAsInputMethodDescriptor();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Finds ComponentExtensionIME and EngineDescription associated with
  // |input_method_id|. This function retruns true if it is found, otherwise
  // returns false. |out_extension| and |out_engine| can be NULL.
  bool FindEngineEntry(const std::string& input_method_id,
                       ComponentExtensionIME* out_extension,
                       ComponentExtensionEngine* out_engine);

  bool IsInLoginLayoutWhitelist(const std::vector<std::string>& layouts);

  scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate_;

  std::vector<ComponentExtensionIME> component_extension_imes_;

  ObserverList<Observer> observers_;

  bool is_initialized_;

  bool was_initialization_notified_;

  std::set<std::string> login_layout_set_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_
