// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_IMPL_H_

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chromeos/ime/component_extension_ime_manager.h"

namespace chromeos {

// The implementation class of ComponentExtentionIMEManagerDelegate.
class ComponentExtentionIMEManagerImpl :
  public ComponentExtentionIMEManagerDelegate {
 public:
  ComponentExtentionIMEManagerImpl();
  virtual ~ComponentExtentionIMEManagerImpl();

  // ComponentExtentionIMEManagerDelegate overrides:
  virtual std::vector<ComponentExtensionIME> ListIME() OVERRIDE;
  virtual bool Load(const std::string& extension_id,
                    const base::FilePath& file_path) OVERRIDE;
  virtual bool Unload(const std::string& extension_id,
                      const base::FilePath& file_path) OVERRIDE;

  // Loads extension list and reads their manifest file. After finished
  // initialization, |callback| will be called on original thread.
  void Initialize(
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner,
      const base::Closure& callback);

  // Returns true if this class is initialized and ready to use, otherwise
  // returns false.
  bool IsInitialized();

 private:
  // Reads component extensions and extract their localized information: name,
  // description and ime id. This function fills them into |out_imes|. This
  // function must be called on file thread.
  static void ReadComponentExtensionsInfo(
      std::vector<ComponentExtensionIME>* out_imes);

  // This function is called on UI thread after ReadComponentExtensionsInfo
  // function is finished. No need to release |result|.
  void OnReadComponentExtensionsInfo(std::vector<ComponentExtensionIME>* result,
                                     const base::Closure& callback);

  // Reads manifest.json file in |file_path|. This function must be called on
  // file thread.
  static scoped_ptr<DictionaryValue> GetManifest(
      const base::FilePath& file_path);

  // Reads extension information: description, option page. This function
  // returns true on success, otherwise returns false. This function must be
  // called on file thread.
  static bool ReadExtensionInfo(const DictionaryValue& manifest,
                                      ComponentExtensionIME* out);

  // Reads each engine component in |dict|. |dict| is given by GetList with
  // kInputComponents key from manifest. This function returns true on success,
  // otherwise retrun false. This function must be called on file thread.
  static bool ReadEngineComponent(const DictionaryValue& dict,
                                  IBusComponent::EngineDescription* out);

  // True if initialized.
  bool is_initialized_;

  // The list of component extension IME.
  std::vector<ComponentExtensionIME> component_extension_list_;

  // The list of already loaded extension ids.
  std::set<std::string> loaded_extension_id_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ComponentExtentionIMEManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtentionIMEManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_IMPL_H_

