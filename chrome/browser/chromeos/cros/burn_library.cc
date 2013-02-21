// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/burn_library.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/observer_list.h"
#include "base/threading/worker_pool.h"
#include "chrome/common/zip.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chromeos {

namespace {

// Unzips |source_zip_file| and sets the filename of the unzipped image to
// |source_image_file|.
void UnzipImage(const base::FilePath& source_zip_file,
                const std::string& image_name,
                scoped_refptr<base::RefCountedString> source_image_file) {
  if (zip::Unzip(source_zip_file, source_zip_file.DirName())) {
    source_image_file->data() =
        source_zip_file.DirName().Append(image_name).value();
  }
}

class BurnLibraryImpl : public BurnLibrary {
 public:
  BurnLibraryImpl();
  virtual ~BurnLibraryImpl();

  // BurnLibrary implementation.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void DoBurn(const base::FilePath& source_path,
                      const std::string& image_name,
                      const base::FilePath& target_file_path,
                      const base::FilePath& target_device_path) OVERRIDE;
  virtual void CancelBurnImage() OVERRIDE;

  void OnImageUnzipped(scoped_refptr<base::RefCountedString> source_image_file);

 private:
  virtual void Init() OVERRIDE;
  void BurnImage();
  void DevicesUnmountedCallback(bool success);
  void UpdateBurnStatus(const ImageBurnStatus& status, BurnEvent evt);
  void OnBurnFinished(const std::string& target_path,
                      bool success,
                      const std::string& error);
  void OnBurnProgressUpdate(const std::string& target_path,
                            int64 num_bytes_burnt,
                            int64 total_size);
  void OnBurnImageFail();

  base::WeakPtrFactory<BurnLibraryImpl> weak_ptr_factory_;

  ObserverList<BurnLibrary::Observer> observers_;

  std::string source_image_file_;
  std::string target_file_path_;
  std::string target_device_path_;

  bool unzipping_;
  bool cancelled_;
  bool burning_;
  bool block_burn_signals_;

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryImpl);
};

BurnLibraryImpl::BurnLibraryImpl()
    : weak_ptr_factory_(this),
      unzipping_(false),
      cancelled_(false),
      burning_(false),
      block_burn_signals_(false) {
}

BurnLibraryImpl::~BurnLibraryImpl() {
  DBusThreadManager::Get()->GetImageBurnerClient()->ResetEventHandlers();
}

void BurnLibraryImpl::Init() {
  DBusThreadManager::Get()->GetImageBurnerClient()->SetEventHandlers(
      base::Bind(&BurnLibraryImpl::OnBurnFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BurnLibraryImpl::OnBurnProgressUpdate,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BurnLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (unzipping_ && !cancelled_)
    UpdateBurnStatus(ImageBurnStatus(), UNZIP_STARTED);
}

void BurnLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BurnLibraryImpl::CancelBurnImage() {
  // At the moment, we cannot really stop uzipping or burning. Instead we
  // prevent events from being send to listeners.
  if (burning_)
    block_burn_signals_ = true;
  cancelled_ = true;
}

void BurnLibraryImpl::DoBurn(const base::FilePath& source_path,
    const std::string& image_name,
    const base::FilePath& target_file_path,
    const base::FilePath& target_device_path) {
  if (unzipping_) {
    // We have unzip in progress, maybe it was "cancelled" before and did not
    // finish yet. In that case, let's pretend cancel did not happen.
    cancelled_ = false;
    UpdateBurnStatus(ImageBurnStatus(), UNZIP_STARTED);
    return;
  }

  source_image_file_.clear();
  target_file_path_ = target_file_path.value();
  target_device_path_ = target_device_path.value();

  unzipping_ = true;
  cancelled_ = false;
  UpdateBurnStatus(ImageBurnStatus(), UNZIP_STARTED);

  const bool task_is_slow = true;
  scoped_refptr<base::RefCountedString> result(new base::RefCountedString);
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(UnzipImage, source_path, image_name, result),
      base::Bind(&BurnLibraryImpl::OnImageUnzipped,
                 weak_ptr_factory_.GetWeakPtr(),
                 result),
      task_is_slow);
}

void BurnLibraryImpl::OnImageUnzipped(
    scoped_refptr<base::RefCountedString> source_image_file) {
  source_image_file_ = source_image_file->data();

  bool success = !source_image_file_.empty();
  UpdateBurnStatus(ImageBurnStatus(), (success ? UNZIP_COMPLETE : UNZIP_FAIL));

  unzipping_ = false;
  if (cancelled_) {
    cancelled_ = false;
    return;
  }

  if (!success)
    return;

  burning_ = true;

  chromeos::disks::DiskMountManager::GetInstance()->UnmountDeviceRecursively(
      target_device_path_,
      base::Bind(&BurnLibraryImpl::DevicesUnmountedCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BurnLibraryImpl::OnBurnFinished(const std::string& target_path,
                                     bool success,
                                     const std::string& error) {
  UpdateBurnStatus(ImageBurnStatus(0, 0), success ? BURN_SUCCESS : BURN_FAIL);
}

void BurnLibraryImpl::OnBurnProgressUpdate(const std::string& target_path,
                                           int64 amount_burnt,
                                           int64 total_size) {
  UpdateBurnStatus(ImageBurnStatus(amount_burnt, total_size), BURN_UPDATE);
}

void BurnLibraryImpl::OnBurnImageFail() {
  UpdateBurnStatus(ImageBurnStatus(0, 0), BURN_FAIL);
}

void BurnLibraryImpl::DevicesUnmountedCallback(bool success) {
  if (success) {
    BurnImage();
  } else {
    UpdateBurnStatus(ImageBurnStatus(0, 0), BURN_FAIL);
  }
}

void BurnLibraryImpl::BurnImage() {
  DBusThreadManager::Get()->GetImageBurnerClient()->BurnImage(
      source_image_file_,
      target_file_path_,
      base::Bind(&BurnLibraryImpl::OnBurnImageFail,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BurnLibraryImpl::UpdateBurnStatus(const ImageBurnStatus& status,
                                       BurnEvent evt) {
  if (cancelled_)
    return;

  if (evt == BURN_FAIL || evt == BURN_SUCCESS) {
    burning_ = false;
    if (block_burn_signals_) {
      block_burn_signals_ = false;
      return;
    }
  }

  if (block_burn_signals_ && evt == BURN_UPDATE)
    return;

  FOR_EACH_OBSERVER(Observer, observers_,
      BurnProgressUpdated(this, evt, status));
}

class BurnLibraryStubImpl : public BurnLibrary {
 public:
  BurnLibraryStubImpl() {}
  virtual ~BurnLibraryStubImpl() {}

  // BurnLibrary overrides.
  virtual void Init() OVERRIDE {}
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual void DoBurn(const base::FilePath& source_path,
                      const std::string& image_name,
                      const base::FilePath& target_file_path,
                      const base::FilePath& target_device_path) OVERRIDE {
  }
  virtual void CancelBurnImage() OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryStubImpl);
};

}  // namespace

// static
BurnLibrary* BurnLibrary::GetImpl(bool stub) {
  BurnLibrary* impl;
  if (stub)
    impl = new BurnLibraryStubImpl();
  else
    impl = new BurnLibraryImpl();
  impl->Init();
  return impl;
}

}  // namespace chromeos
