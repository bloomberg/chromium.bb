// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unzip_service/public/cpp/unzip.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/unzip_service/public/interfaces/constants.mojom.h"
#include "components/unzip_service/public/interfaces/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"

namespace unzip {

namespace {

class UnzipParams : public base::RefCounted<UnzipParams> {
 public:
  UnzipParams(mojom::UnzipperPtr unzipper, UnzipCallback callback)
      : unzipper_(std::move(unzipper)), callback_(std::move(callback)) {}

  mojom::UnzipperPtr* unzipper() { return &unzipper_; }

  UnzipCallback TakeCallback() { return std::move(callback_); }

 private:
  friend class base::RefCounted<UnzipParams>;

  ~UnzipParams() = default;

  // The UnzipperPtr is stored so it does not get deleted before the callback
  // runs.
  mojom::UnzipperPtr unzipper_;

  UnzipCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(UnzipParams);
};

void UnzipDone(scoped_refptr<UnzipParams> params, bool success) {
  UnzipCallback cb = params->TakeCallback();
  if (!cb.is_null())
    std::move(cb).Run(success);
}

}  // namespace

void Unzip(service_manager::Connector* connector,
           const base::FilePath& zip_path,
           const base::FilePath& output_dir,
           UnzipCallback callback) {
  DCHECK(!callback.is_null());

  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!zip_file.IsValid()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  filesystem::mojom::DirectoryPtr directory_ptr;
  mojo::MakeStrongBinding(
      std::make_unique<filesystem::DirectoryImpl>(output_dir, nullptr, nullptr),
      mojo::MakeRequest(&directory_ptr));

  mojom::UnzipperPtr unzipper;
  connector->BindInterface(mojom::kServiceName, mojo::MakeRequest(&unzipper));

  // |callback| is shared between the connection error handler and the Unzip
  // call using a refcounted UnzipParams object that owns |callback|.
  auto unzip_params = base::MakeRefCounted<UnzipParams>(std::move(unzipper),
                                                        std::move(callback));

  unzip_params->unzipper()->set_connection_error_handler(
      base::BindRepeating(&UnzipDone, unzip_params, false));
  (*unzip_params->unzipper())
      ->Unzip(std::move(zip_file), std::move(directory_ptr),
              base::BindOnce(&UnzipDone, unzip_params));
}

}  // namespace unzip
