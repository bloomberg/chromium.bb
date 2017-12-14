// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ASSETS_H_
#define CHROME_BROWSER_VR_ASSETS_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/vr/assets_load_status.h"

class SkBitmap;

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
class Version;
}  // namespace base

namespace vr {

constexpr uint32_t kCompatibleMajorVrAssetsComponentVersion = 1;

class MetricsHelper;
struct AssetsSingletonTrait;

// Manages VR assets such as the environment. Gets updated by the VR assets
// component.
//
// All function are thread-safe. The reason is that the component will be made
// available on a different thread than the asset load request. Internally, the
// function calls will be posted on the main thread. The asset load may be
// performed on a worker thread.
class Assets {
 public:
  typedef base::OnceCallback<void(AssetsLoadStatus status,
                                  std::unique_ptr<SkBitmap> background_image,
                                  const base::Version& component_version)>
      OnAssetsLoadedCallback;

  // Returns the single assets instance and creates it on first call.
  static Assets* GetInstance();

  // Tells VR assets that a new VR assets component version is ready for use.
  void OnComponentReady(const base::Version& version,
                        const base::FilePath& install_dir,
                        std::unique_ptr<base::DictionaryValue> manifest);

  // Calls |on_loaded| when a component version is ready and the containing
  // assets are loaded. |on_loaded| will be called on the caller's thread.
  void LoadWhenComponentReady(OnAssetsLoadedCallback on_loaded);

  MetricsHelper* GetMetricsHelper();

 private:
  static void LoadAssetsTask(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::Version& component_version,
      const base::FilePath& component_install_dir,
      OnAssetsLoadedCallback on_loaded);

  Assets();
  ~Assets();
  void OnComponentReadyInternal(const base::Version& version,
                                const base::FilePath& install_dir);
  void LoadWhenComponentReadyInternal(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      OnAssetsLoadedCallback on_loaded);

  bool component_ready_ = false;
  base::Version component_version_;
  base::FilePath component_install_dir_;
  OnAssetsLoadedCallback on_assets_loaded_;
  scoped_refptr<base::SingleThreadTaskRunner> on_assets_loaded_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<MetricsHelper> metrics_helper_;

  base::WeakPtrFactory<Assets> weak_ptr_factory_;

  friend struct AssetsSingletonTrait;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ASSETS_H_
