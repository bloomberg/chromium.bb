// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/lorgnette_manager_client.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "net/base/file_stream.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The LorgnetteManagerClient implementation used in production.
class LorgnetteManagerClientImpl : public LorgnetteManagerClient {
 public:
  LorgnetteManagerClientImpl() :
      lorgnette_daemon_proxy_(NULL), weak_ptr_factory_(this) {}

  ~LorgnetteManagerClientImpl() override {}

  void ListScanners(const ListScannersCallback& callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kListScannersMethod);
    lorgnette_daemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&LorgnetteManagerClientImpl::OnListScanners,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // LorgnetteManagerClient override.
  void ScanImageToFile(
      std::string device_name,
      const ScanProperties& properties,
      const ScanImageToFileCallback& callback,
      base::File* file) override {
    dbus::FileDescriptor* file_descriptor = new dbus::FileDescriptor();
    file_descriptor->PutValue(file->TakePlatformFile());
    // Punt descriptor validity check to a worker thread; on return we'll
    // issue the D-Bus request to stop tracing and collect results.
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&LorgnetteManagerClientImpl::CheckValidity,
                   file_descriptor),
        base::Bind(&LorgnetteManagerClientImpl::OnCheckValidityScanImage,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(file_descriptor),
                   device_name,
                   properties,
                   callback),
        false);
  }

  void ScanImageToString(
      std::string device_name,
      const ScanProperties& properties,
      const ScanImageToStringCallback& callback) override {
    // Owned by the callback created in scan_to_string_completion->Start().
    ScanToStringCompletion* scan_to_string_completion =
        new ScanToStringCompletion();
    base::File file;
    ScanImageToFileCallback file_callback =
        scan_to_string_completion->Start(callback, &file);
    ScanImageToFile(device_name, properties, file_callback, &file);
  }

 protected:
  void Init(dbus::Bus* bus) override {
    lorgnette_daemon_proxy_ =
        bus->GetObjectProxy(lorgnette::kManagerServiceName,
                            dbus::ObjectPath(lorgnette::kManagerServicePath));
  }

 private:
  class ScanToStringCompletion {
   public:
    ScanToStringCompletion() {}
    virtual ~ScanToStringCompletion() {}

    // Creates a file stream in |file| that will stream image data to
    // a string that will be supplied to |callback|.  Passes ownership
    // of |this| to a returned callback that can be handed to a
    // ScanImageToFile invocation.
    ScanImageToFileCallback Start(const ScanImageToStringCallback& callback,
                                  base::File *file) {
      CHECK(!pipe_reader_.get());
      const bool kTasksAreSlow = true;
      scoped_refptr<base::TaskRunner> task_runner =
          base::WorkerPool::GetTaskRunner(kTasksAreSlow);
      pipe_reader_.reset(
          new chromeos::PipeReaderForString(
              task_runner,
              base::Bind(&ScanToStringCompletion::OnScanToStringDataCompleted,
                       base::Unretained(this))));
      *file = pipe_reader_->StartIO();

      return base::Bind(&ScanToStringCompletion::OnScanToStringCompleted,
                        base::Owned(this), callback);
    }

   private:
    // Called when a |pipe_reader_| completes reading scan data to a string.
    void OnScanToStringDataCompleted() {
      pipe_reader_->GetData(&scanned_image_data_string_);
      pipe_reader_.reset();
    }

    // Called by LorgnetteManagerImpl when scan completes.
    void OnScanToStringCompleted(const ScanImageToStringCallback& callback,
                                 bool succeeded) {
      if (pipe_reader_.get()) {
        pipe_reader_->OnDataReady(-1);  // terminate data stream
      }
      callback.Run(succeeded, scanned_image_data_string_);
      scanned_image_data_string_.clear();
    }

    scoped_ptr<chromeos::PipeReaderForString> pipe_reader_;
    std::string scanned_image_data_string_;

    DISALLOW_COPY_AND_ASSIGN(ScanToStringCompletion);
  };

  // Called when ListScanners completes.
  void OnListScanners(const ListScannersCallback& callback,
                      dbus::Response* response) {
    ScannerTable scanners;
    dbus::MessageReader table_reader(NULL);
    if (!response || !dbus::MessageReader(response).PopArray(&table_reader)) {
      callback.Run(false, scanners);
      return;
    }

    bool decode_failure = false;
    while (table_reader.HasMoreData()) {
      std::string device_name;
      dbus::MessageReader device_entry_reader(NULL);
      dbus::MessageReader device_element_reader(NULL);
      if (!table_reader.PopDictEntry(&device_entry_reader) ||
          !device_entry_reader.PopString(&device_name) ||
          !device_entry_reader.PopArray(&device_element_reader)) {
        decode_failure = true;
        break;
      }

      ScannerTableEntry scanner_entry;
      while (device_element_reader.HasMoreData()) {
        dbus::MessageReader device_attribute_reader(NULL);
        std::string attribute;
        std::string value;
        if (!device_element_reader.PopDictEntry(&device_attribute_reader) ||
            !device_attribute_reader.PopString(&attribute) ||
            !device_attribute_reader.PopString(&value)) {
          decode_failure = true;
          break;
        }
        scanner_entry[attribute] = value;
      }

      if (decode_failure)
          break;

      scanners[device_name] = scanner_entry;
    }

    if (decode_failure) {
      LOG(ERROR) << "Failed to decode response from ListScanners";
      callback.Run(false, scanners);
    } else {
      callback.Run(true, scanners);
    }
  }

  // Called to check descriptor validity on a thread where i/o is permitted.
  static void CheckValidity(dbus::FileDescriptor* file_descriptor) {
    file_descriptor->CheckValidity();
  }

  // Called when a CheckValidity response is received.
  void OnCheckValidityScanImage(
      dbus::FileDescriptor* file_descriptor,
      std::string device_name,
      const ScanProperties& properties,
      const ScanImageToFileCallback& callback) {
    if (!file_descriptor->is_valid()) {
      LOG(ERROR) << "Failed to scan image: file descriptor is invalid";
      callback.Run(false);
      return;
    }
    // Issue the dbus request to scan an image.
    dbus::MethodCall method_call(
        lorgnette::kManagerServiceInterface,
        lorgnette::kScanImageMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_name);
    writer.AppendFileDescriptor(*file_descriptor);

    dbus::MessageWriter option_writer(NULL);
    dbus::MessageWriter element_writer(NULL);
    writer.OpenArray("{sv}", &option_writer);
    if (!properties.mode.empty()) {
      option_writer.OpenDictEntry(&element_writer);
      element_writer.AppendString(lorgnette::kScanPropertyMode);
      element_writer.AppendVariantOfString(properties.mode);
      option_writer.CloseContainer(&element_writer);
    }
    if (properties.resolution_dpi) {
      option_writer.OpenDictEntry(&element_writer);
      element_writer.AppendString(lorgnette::kScanPropertyResolution);
      element_writer.AppendVariantOfUint32(properties.resolution_dpi);
      option_writer.CloseContainer(&element_writer);
    }
    writer.CloseContainer(&option_writer);

    lorgnette_daemon_proxy_->CallMethod(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&LorgnetteManagerClientImpl::OnScanImageComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  }

  // Called when a response for ScanImage() is received.
  void OnScanImageComplete(const ScanImageToFileCallback& callback,
                           dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to scan image";
      callback.Run(false);
      return;
    }
    callback.Run(true);
  }

  dbus::ObjectProxy* lorgnette_daemon_proxy_;
  base::WeakPtrFactory<LorgnetteManagerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LorgnetteManagerClientImpl);
};

LorgnetteManagerClient::LorgnetteManagerClient() {
}

LorgnetteManagerClient::~LorgnetteManagerClient() {
}

// static
LorgnetteManagerClient* LorgnetteManagerClient::Create() {
  return new LorgnetteManagerClientImpl();
}

}  // namespace chromeos
