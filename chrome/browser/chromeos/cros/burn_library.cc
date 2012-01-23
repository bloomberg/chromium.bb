// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/burn_library.h"

#include <cstring>

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#include "chrome/common/zip.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

class BurnLibraryImpl : public BurnLibrary,
                        public base::SupportsWeakPtr<BurnLibraryImpl> {
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

  // Called by BurnLibraryTaskProxy.
  void UnzipImage();
  void OnImageUnzipped();

 private:
  void Init();
  void BurnImage();
  static void DevicesUnmountedCallback(void* object, bool success);
  void UpdateBurnStatus(const ImageBurnStatus& status, BurnEvent evt);
  static void BurnStatusChangedHandler(void* object,
                                       const BurnStatus& status,
                                       BurnEventType evt);

 private:
  ObserverList<BurnLibrary::Observer> observers_;
  BurnStatusConnection burn_status_connection_;

  FilePath source_zip_file_;
  std::string source_image_file_;
  std::string source_image_name_;
  std::string target_file_path_;
  std::string target_device_path_;

  bool unzipping_;
  bool cancelled_;
  bool burning_;
  bool block_burn_signals_;

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryImpl);
};

class BurnLibraryTaskProxy
    : public base::RefCountedThreadSafe<BurnLibraryTaskProxy> {
 public:
  explicit BurnLibraryTaskProxy(const base::WeakPtr<BurnLibraryImpl>& library)
      : library_(library) {
    library_->DetachFromThread();
  }

  void UnzipImage() {
    if (library_)
      library_->UnzipImage();
  }

  void ImageUnzipped() {
    if (library_)
      library_->OnImageUnzipped();
  }

 private:
  base::WeakPtr<BurnLibraryImpl> library_;

  friend class base::RefCountedThreadSafe<BurnLibraryTaskProxy>;

  DISALLOW_COPY_AND_ASSIGN(BurnLibraryTaskProxy);
};

BurnLibraryImpl::BurnLibraryImpl() : unzipping_(false),
                                     cancelled_(false),
                                     burning_(false),
                                     block_burn_signals_(false) {
}

BurnLibraryImpl::~BurnLibraryImpl() {
  if (burn_status_connection_) {
    DisconnectBurnStatus(burn_status_connection_);
  }
}

void BurnLibraryImpl::Init() {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  burn_status_connection_ =
      chromeos::MonitorBurnStatus(&BurnStatusChangedHandler, this);
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

  source_zip_file_ = source_path;
  source_image_file_.clear();
  source_image_name_ = image_name;
  target_file_path_ = target_file_path.value();
  target_device_path_ = target_device_path.value();

  unzipping_ = true;
  cancelled_ = false;
  UpdateBurnStatus(ImageBurnStatus(), UNZIP_STARTED);

  scoped_refptr<BurnLibraryTaskProxy> task =
      new BurnLibraryTaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&BurnLibraryTaskProxy::UnzipImage,
                                     task));
}

void BurnLibraryImpl::UnzipImage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (zip::Unzip(source_zip_file_, source_zip_file_.DirName())) {
    source_image_file_ =
        source_zip_file_.DirName().Append(source_image_name_).value();
  }

  scoped_refptr<BurnLibraryTaskProxy> task =
      new BurnLibraryTaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&BurnLibraryTaskProxy::ImageUnzipped,
                                     task));
}

void BurnLibraryImpl::OnImageUnzipped() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

void BurnLibraryImpl::DevicesUnmountedCallback(void* object, bool success) {
  DCHECK(object);
  BurnLibraryImpl* self = static_cast<BurnLibraryImpl*>(object);
  if (success) {
    self->BurnImage();
  } else {
    self->UpdateBurnStatus(ImageBurnStatus(self->target_file_path_.c_str(),
        0, 0, "Error unmounting device"), BURN_FAIL);
  }
}

void BurnLibraryImpl::BurnImage() {
  RequestBurn(source_image_file_.c_str(), target_file_path_.c_str(),
      &BurnLibraryImpl::BurnStatusChangedHandler, this);
}

void BurnLibraryImpl::BurnStatusChangedHandler(void* object,
    const BurnStatus& status, BurnEventType evt) {
  DCHECK(object);
  BurnLibraryImpl* self = static_cast<BurnLibraryImpl*>(object);

// Copy burn status because it will be freed after returning from this method.
  ImageBurnStatus status_copy(status);
  BurnEvent event = UNKNOWN;
  switch (evt) {
    case(BURN_CANCELED):
      event = BURN_FAIL;
      break;
    case(BURN_COMPLETE):
      event = BURN_SUCCESS;
      break;
    case(BURN_UPDATED):
    case(BURN_STARTED):
      event = BURN_UPDATE;
      break;
  }
  self->UpdateBurnStatus(status_copy, event);
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
