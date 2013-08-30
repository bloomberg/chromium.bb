// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/resource_cache.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace policy {

namespace {

// Verifies that |value| is not empty and encodes it into base64url format,
// which is safe to use as a file name on all platforms.
bool Base64Encode(const std::string& value, std::string* encoded) {
  DCHECK(!value.empty());
  if (value.empty() || !base::Base64Encode(value, encoded))
    return false;
  ReplaceChars(*encoded, "+", "-", encoded);
  ReplaceChars(*encoded, "/", "_", encoded);
  return true;
}

// Decodes all elements of |input| from base64url format and stores the decoded
// elements in |output|.
bool Base64Encode(const std::set<std::string>& input,
                  std::set<std::string>* output) {
  output->clear();
  for (std::set<std::string>::const_iterator it = input.begin();
       it != input.end(); ++it) {
    std::string encoded;
    if (!Base64Encode(*it, &encoded)) {
      output->clear();
      return false;
    }
    output->insert(encoded);
  }
  return true;
}

// Decodes |encoded| from base64url format and verifies that the result is not
// emtpy.
bool Base64Decode(const std::string& encoded, std::string* value) {
  std::string buffer;
  ReplaceChars(encoded, "-", "+", &buffer);
  ReplaceChars(buffer, "_", "/", &buffer);
  return base::Base64Decode(buffer, value) && !value->empty();
}

}  // namespace

ResourceCache::ResourceCache(const base::FilePath& cache_dir)
    : cache_dir_(cache_dir) {
  // Allow the cache to be created in a different thread than the thread that is
  // going to use it.
  DetachFromThread();
}

ResourceCache::~ResourceCache() {
  DCHECK(CalledOnValidThread());
}

bool ResourceCache::Store(const std::string& key,
                          const std::string& subkey,
                          const std::string& data) {
  DCHECK(CalledOnValidThread());
  base::FilePath subkey_path;
  // Delete the file before writing to it. This ensures that the write does not
  // follow a symlink planted at |subkey_path|, clobbering a file outside the
  // cache directory. The mechanism is meant to foil file-system-level attacks
  // where a symlink is planted in the cache directory before Chrome has
  // started. An attacker controlling a process running concurrently with Chrome
  // would be able to race against the protection by re-creating the symlink
  // between these two calls. There is nothing in file_util that could be used
  // to protect against such races, especially as the cache is cross-platform
  // and therefore cannot use any POSIX-only tricks.
  return VerifyKeyPathAndGetSubkeyPath(key, true, subkey, &subkey_path) &&
         base::DeleteFile(subkey_path, false) &&
         file_util::WriteFile(subkey_path, data.data(), data.size());
}

bool ResourceCache::Load(const std::string& key,
                         const std::string& subkey,
                         std::string* data) {
  DCHECK(CalledOnValidThread());
  base::FilePath subkey_path;
  // Only read from |subkey_path| if it is not a symlink.
  if (!VerifyKeyPathAndGetSubkeyPath(key, false, subkey, &subkey_path) ||
      file_util::IsLink(subkey_path)) {
    return false;
  }
  data->clear();
  return base::ReadFileToString(subkey_path, data);
}

void ResourceCache::LoadAllSubkeys(
    const std::string& key,
    std::map<std::string, std::string>* contents) {
  DCHECK(CalledOnValidThread());
  contents->clear();
  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string encoded_subkey = path.BaseName().MaybeAsASCII();
    std::string subkey;
    std::string data;
    // Only read from |subkey_path| if it is not a symlink and its name is
    // a base64-encoded string.
    if (!file_util::IsLink(path) &&
        Base64Decode(encoded_subkey, &subkey) &&
        base::ReadFileToString(path, &data)) {
      (*contents)[subkey].swap(data);
    }
  }
}

void ResourceCache::Delete(const std::string& key, const std::string& subkey) {
  DCHECK(CalledOnValidThread());
  base::FilePath subkey_path;
  if (VerifyKeyPathAndGetSubkeyPath(key, false, subkey, &subkey_path))
    base::DeleteFile(subkey_path, false);
  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if its last subkey was
  // just removed and does nothing otherwise.
  base::DeleteFile(subkey_path.DirName(), false);
}

void ResourceCache::PurgeOtherKeys(const std::set<std::string>& keys_to_keep) {
  DCHECK(CalledOnValidThread());
  std::set<std::string> encoded_keys_to_keep;
  if (!Base64Encode(keys_to_keep, &encoded_keys_to_keep))
    return;

  base::FileEnumerator enumerator(
      cache_dir_, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name(path.BaseName().MaybeAsASCII());
    if (encoded_keys_to_keep.find(name) == encoded_keys_to_keep.end())
      base::DeleteFile(path, true);
  }
}

void ResourceCache::PurgeOtherSubkeys(
    const std::string& key,
    const std::set<std::string>& subkeys_to_keep) {
  DCHECK(CalledOnValidThread());
  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  std::set<std::string> encoded_subkeys_to_keep;
  if (!Base64Encode(subkeys_to_keep, &encoded_subkeys_to_keep))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name(path.BaseName().MaybeAsASCII());
    if (encoded_subkeys_to_keep.find(name) == encoded_subkeys_to_keep.end())
      base::DeleteFile(path, false);
  }
  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if all of its subkeys
  // were just removed and does nothing otherwise.
  base::DeleteFile(key_path, false);
}

bool ResourceCache::VerifyKeyPath(const std::string& key,
                                  bool allow_create,
                                  base::FilePath* path) {
  std::string encoded;
  if (!Base64Encode(key, &encoded))
    return false;
  *path = cache_dir_.AppendASCII(encoded);
  return allow_create ? file_util::CreateDirectory(*path) :
                        base::DirectoryExists(*path);
}

bool ResourceCache::VerifyKeyPathAndGetSubkeyPath(const std::string& key,
                                                  bool allow_create_key,
                                                  const std::string& subkey,
                                                  base::FilePath* path) {
  base::FilePath key_path;
  std::string encoded;
  if (!VerifyKeyPath(key, allow_create_key, &key_path) ||
      !Base64Encode(subkey, &encoded)) {
    return false;
  }
  *path = key_path.AppendASCII(encoded);
  return true;
}


}  // namespace policy
