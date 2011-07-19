// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ZIP_H_
#define CHROME_COMMON_ZIP_H_
#pragma once

class FilePath;

// Zip the contents of src_dir into dest_file. src_path must be a directory.
// An entry will *not* be created in the zip for the root folder -- children
// of src_dir will be at the root level of the created zip.
// If |include_hidden_files| is true, files starting with "." are included.
// Otherwise they are omitted.
bool Zip(const FilePath& src_dir, const FilePath& dest_file,
         bool include_hidden_files);

// Unzip the contents of zip_file into dest_dir.
bool Unzip(const FilePath& zip_file, const FilePath& dest_dir);

#endif  // CHROME_COMMON_ZIP_H_
