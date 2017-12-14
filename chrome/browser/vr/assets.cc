// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/assets.h"

#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/vr/metrics_helper.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace vr {

namespace {

static const base::FilePath::CharType kBackgroundFileNamePng[] =
    FILE_PATH_LITERAL("background.png");
static const base::FilePath::CharType kBackgroundFileNameJpeg[] =
    FILE_PATH_LITERAL("background.jpeg");

}  // namespace

struct AssetsSingletonTrait : public base::DefaultSingletonTraits<Assets> {
  static Assets* New() { return new Assets(); }
  static void Delete(Assets* assets) { delete assets; }
};

// static
Assets* Assets::GetInstance() {
  return base::Singleton<Assets, AssetsSingletonTrait>::get();
}

void Assets::OnComponentReady(const base::Version& version,
                              const base::FilePath& install_dir,
                              std::unique_ptr<base::DictionaryValue> manifest) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Assets::OnComponentReadyInternal,
                     weak_ptr_factory_.GetWeakPtr(), version, install_dir));
}

void Assets::LoadWhenComponentReady(OnAssetsLoadedCallback on_loaded) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Assets::LoadWhenComponentReadyInternal,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get(),
                                std::move(on_loaded)));
}

MetricsHelper* Assets::GetMetricsHelper() {
  // If we instantiate metrics_helper_ in the constructor all functions of
  // MetricsHelper must be called in a valid sequence from the thread the
  // constructor ran on. However, the assets class can be instantiated from any
  // thread. To avoid the aforementioned restriction, create metrics_helper_ the
  // first time it is used and, thus, give the caller control over when the
  // sequence starts.
  if (!metrics_helper_) {
    metrics_helper_ = base::MakeUnique<MetricsHelper>();
  }
  return metrics_helper_.get();
}

// static
void Assets::LoadAssetsTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Version& component_version,
    const base::FilePath& component_install_dir,
    OnAssetsLoadedCallback on_loaded) {
  std::string background_file_content;
  base::FilePath background_file_path;
  bool is_png = false;

  if (base::PathExists(component_install_dir.Append(kBackgroundFileNamePng))) {
    background_file_path = component_install_dir.Append(kBackgroundFileNamePng);
    is_png = true;
  } else if (base::PathExists(
                 component_install_dir.Append(kBackgroundFileNameJpeg))) {
    background_file_path =
        component_install_dir.Append(kBackgroundFileNameJpeg);
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_loaded), AssetsLoadStatus::kNotFound,
                       nullptr, component_version));
    return;
  }

  if (!base::ReadFileToString(background_file_path, &background_file_content)) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_loaded), AssetsLoadStatus::kParseFailure,
                       nullptr, component_version));
    return;
  }

  std::unique_ptr<SkBitmap> background_image;
  if (is_png) {
    background_image = base::MakeUnique<SkBitmap>();
    if (!gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(
                                   background_file_content.data()),
                               background_file_content.size(),
                               background_image.get())) {
      background_image = nullptr;
    }
  } else {
    background_image = gfx::JPEGCodec::Decode(
        reinterpret_cast<const unsigned char*>(background_file_content.data()),
        background_file_content.size());
  }

  if (!background_image) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_loaded), AssetsLoadStatus::kInvalidContent,
                       nullptr, component_version));
    return;
  }

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_loaded), AssetsLoadStatus::kSuccess,
                     std::move(background_image), component_version));
}

Assets::Assets()
    : main_thread_task_runner_(content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI)),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_task_runner_.get());
}

Assets::~Assets() = default;

void Assets::OnComponentReadyInternal(const base::Version& version,
                                      const base::FilePath& install_dir) {
  component_version_ = version;
  component_install_dir_ = install_dir;
  component_ready_ = true;
  GetMetricsHelper()->OnComponentReady(version);
  if (on_assets_loaded_) {
    LoadWhenComponentReadyInternal(on_assets_loaded_task_runner_,
                                   std::move(on_assets_loaded_));
    on_assets_loaded_task_runner_ = nullptr;
  }
}

void Assets::LoadWhenComponentReadyInternal(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    OnAssetsLoadedCallback on_loaded) {
  if (component_ready_) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
        base::BindOnce(&Assets::LoadAssetsTask, task_runner, component_version_,
                       component_install_dir_, std::move(on_loaded)));
  } else {
    on_assets_loaded_ = std::move(on_loaded);
    on_assets_loaded_task_runner_ = task_runner;
  }
}

}  // namespace vr
