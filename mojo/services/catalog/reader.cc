// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/catalog/reader.h"

#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "mojo/services/catalog/entry.h"
#include "mojo/shell/public/cpp/names.h"

namespace catalog {
namespace {

scoped_ptr<base::Value> ReadManifest(const base::FilePath& manifest_path) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;
  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return deserializer.Deserialize(&error, &message);
}

void OnReadManifest(base::WeakPtr<Reader> reader,
                    const std::string& name,
                    const Reader::ReadManifestCallback& callback,
                    scoped_ptr<base::Value> manifest) {
  if (!reader) {
    // The Reader was destroyed, we're likely in shutdown. Run the callback so
    // we don't trigger a DCHECK.
    callback.Run(nullptr);
    return;
  }
  scoped_ptr<Entry> entry;
  if (manifest) {
    const base::DictionaryValue* dictionary = nullptr;
    CHECK(manifest->GetAsDictionary(&dictionary));
    entry = Entry::Deserialize(*dictionary);
  }
  callback.Run(std::move(entry));
}

}  // namespace

Reader::Reader(const base::FilePath& package_path,
               base::TaskRunner* file_task_runner)
    : package_path_(package_path),
      file_task_runner_(file_task_runner),
      weak_factory_(this) {}
Reader::~Reader() {}

void Reader::Read(const std::string& name,
                  const ReadManifestCallback& callback) {
  base::FilePath manifest_path = GetManifestPath(name);
  if (manifest_path.empty()) {
    callback.Run(nullptr);
    return;
  }

  std::string type = mojo::GetNameType(name);
  CHECK(type == "mojo" || type == "exe");
  base::PostTaskAndReplyWithResult(
      file_task_runner_, FROM_HERE, base::Bind(&ReadManifest, manifest_path),
      base::Bind(&OnReadManifest, weak_factory_.GetWeakPtr(), name, callback));
}

base::FilePath Reader::GetManifestPath(const std::string& name) const {
  // TODO(beng): think more about how this should be done for exe targets.
  std::string type = mojo::GetNameType(name);
  std::string path = mojo::GetNamePath(name);
  if (type == "mojo")
    return package_path_.AppendASCII(path + "/manifest.json");
  else if (type == "exe")
    return package_path_.AppendASCII(path + "_manifest.json");
  return base::FilePath();
}

}  // namespace catalog
