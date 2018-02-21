// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unzip_service/unzipper_impl.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/zlib/google/zip.h"
#include "third_party/zlib/google/zip_reader.h"

namespace unzip {

namespace {

// A writer delegate that reports errors instead of writing.
class DudWriterDelegate : public zip::WriterDelegate {
 public:
  DudWriterDelegate() {}
  ~DudWriterDelegate() override {}

  // WriterDelegate methods:
  bool PrepareOutput() override { return false; }
  bool WriteBytes(const char* data, int num_bytes) override { return false; }
  void SetTimeModified(const base::Time& time) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DudWriterDelegate);
};

std::string PathToMojoString(const base::FilePath& path) {
#if defined(OS_WIN)
  return base::WideToUTF8(path.value());
#else
  return path.value();
#endif
}

// Modifies output_dir to point to the final directory.
bool CreateDirectory(filesystem::mojom::DirectoryPtr* output_dir,
                     const base::FilePath& path) {
  filesystem::mojom::FileError err = filesystem::mojom::FileError::OK;
  return (*output_dir)
             ->OpenDirectory(PathToMojoString(path), nullptr,
                             filesystem::mojom::kFlagOpenAlways, &err) &&
         err == filesystem::mojom::FileError::OK;
}

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegateNoParent(
    filesystem::mojom::DirectoryPtr* output_dir,
    const base::FilePath& path) {
  auto file = std::make_unique<base::File>();
  filesystem::mojom::FileError err;
  if (!(*output_dir)
           ->OpenFileHandle(PathToMojoString(path),
                            filesystem::mojom::kFlagCreate |
                                filesystem::mojom::kFlagWrite |
                                filesystem::mojom::kFlagWriteAttributes,
                            &err, file.get()) ||
      err != filesystem::mojom::FileError::OK) {
    return std::make_unique<DudWriterDelegate>();
  }
  return std::make_unique<zip::FileWriterDelegate>(std::move(file));
}

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegate(
    filesystem::mojom::DirectoryPtr* output_dir,
    const base::FilePath& path) {
  if (path == path.BaseName())
    return MakeFileWriterDelegateNoParent(output_dir, path);
  filesystem::mojom::DirectoryPtr parent;
  filesystem::mojom::FileError err;
  if (!(*output_dir)
           ->OpenDirectory(PathToMojoString(path.DirName()),
                           mojo::MakeRequest(&parent),
                           filesystem::mojom::kFlagOpenAlways, &err) ||
      err != filesystem::mojom::FileError::OK) {
    return std::make_unique<DudWriterDelegate>();
  }
  return MakeFileWriterDelegateNoParent(&parent, path.BaseName());
}

bool FilterNoFiles(const base::FilePath& unused) {
  return true;
}

}  // namespace

UnzipperImpl::UnzipperImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

UnzipperImpl::~UnzipperImpl() = default;

void UnzipperImpl::Unzip(base::File zip_file,
                         filesystem::mojom::DirectoryPtr output_dir,
                         UnzipCallback callback) {
  DCHECK(zip_file.IsValid());
  std::move(callback).Run(zip::UnzipWithFilterAndWriters(
      zip_file.GetPlatformFile(),
      base::BindRepeating(&MakeFileWriterDelegate, &output_dir),
      base::BindRepeating(&CreateDirectory, &output_dir),
      base::BindRepeating(&FilterNoFiles), false));
}

}  // namespace unzip
