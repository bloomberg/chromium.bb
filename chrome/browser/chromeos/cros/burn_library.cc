// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/burn_library.h"

#include <cstring>

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/image_burner_client.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#include "chrome/common/zip.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

void UnzipImage(
    const FilePath& source_zip_file,
    const std::string& image_name,
    base::Callback<void(const std::string& source_image_file)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string source_image_file;
  if (zip::Unzip(source_zip_file, source_zip_file.DirName())) {
    source_image_file =
        source_zip_file.DirName().Append(image_name).value();
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, source_image_file));
}

}  // namespace

class BurnLibraryImpl : public BurnLibrary {
 public:
  BurnLibraryImpl();
  virtual ~BurnLibraryImpl();

  // BurnLibrary implementation.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void DoBurn(const FilePath& source_path,
                      const std::string& image_name,
                      const FilePath& target_file_path,
                      const FilePath& target_device_path) OVERRIDE;
  virtual void CancelBurnImage() OVERRIDE;

  void OnImageUnzipped(const std::string& source_image_file);

 private:
  void Init();
  void BurnImage();
  static void DevicesUnmountedCallback(void* object, bool success);
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

void BurnLibraryImpl::DoBurn(const FilePath& source_path,
    const std::string& image_name,
    const FilePath& target_file_path,
    const FilePath& target_device_path) {
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

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(UnzipImage,
                 source_path,
                 image_name,
                 base::Bind(&BurnLibraryImpl::OnImageUnzipped,
                            weak_ptr_factory_.GetWeakPtr())));
}

void BurnLibraryImpl::OnImageUnzipped(const std::string& source_image_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  source_image_file_ = source_image_file;

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

  chromeos::disks::DiskMountManager::GetInstance()->
      UnmountDeviceRecursive(target_device_path_,
                             &BurnLibraryImpl::DevicesUnmountedCallback,
                             this);
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

void BurnLibraryImpl::DevicesUnmountedCallback(void* object, bool success) {
  DCHECK(object);
  BurnLibraryImpl* self = static_cast<BurnLibraryImpl*>(object);
  if (success) {
    self->BurnImage();
  } else {
    self->UpdateBurnStatus(ImageBurnStatus(0, 0), BURN_FAIL);
  }
}

void BurnLibraryImpl::BurnImage() {
  DBusThreadManager::Get()->GetImageBurnerClient()->BurnImage(
      source_image_file_.c_str(),
      target_file_path_.c_str(),
      base::Bind(&BurnLibraryImpl::OnBurnImageFail,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BurnLibraryImpl::UpdateBurnStatus(const ImageBurnStatus& status,
                                       BurnEvent evt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  virtual void DoBurn(const FilePath& source_path,
                      const std::string& image_name,
                      const FilePath& target_file_path,
                      const FilePath& target_device_path) OVERRIDE {
  }
  virtual void CancelBurnImage() OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryStubImpl);
};

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
