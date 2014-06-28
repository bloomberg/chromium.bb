// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "crypto/sha2.h"
#include "extensions/browser/computed_hashes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ComputedHashes, ComputedHashes) {
  base::ScopedTempDir scoped_dir;
  ASSERT_TRUE(scoped_dir.CreateUniqueTempDir());
  base::FilePath computed_hashes =
      scoped_dir.path().AppendASCII("computed_hashes.json");

  // We'll add hashes for 2 files, one of which uses a subdirectory
  // path. The first file will have a list of 1 block hash, and the
  // second file will have 2 block hashes.
  base::FilePath path1(FILE_PATH_LITERAL("foo.txt"));
  base::FilePath path2 =
      base::FilePath(FILE_PATH_LITERAL("foo")).AppendASCII("bar.txt");
  std::vector<std::string> hashes1;
  std::vector<std::string> hashes2;
  hashes1.push_back(crypto::SHA256HashString("first"));
  hashes2.push_back(crypto::SHA256HashString("second"));
  hashes2.push_back(crypto::SHA256HashString("third"));

  // Write them into the file.
  ComputedHashes::Writer writer;
  writer.AddHashes(path1, 4096, hashes1);
  writer.AddHashes(path2, 2048, hashes2);
  EXPECT_TRUE(writer.WriteToFile(computed_hashes));

  // Now read them back again and assert that we got what we wrote.
  ComputedHashes::Reader reader;
  std::vector<std::string> read_hashes1;
  std::vector<std::string> read_hashes2;
  EXPECT_TRUE(reader.InitFromFile(computed_hashes));

  int block_size = 0;
  EXPECT_TRUE(reader.GetHashes(path1, &block_size, &read_hashes1));
  EXPECT_EQ(block_size, 4096);
  block_size = 0;
  EXPECT_TRUE(reader.GetHashes(path2, &block_size, &read_hashes2));
  EXPECT_EQ(block_size, 2048);

  EXPECT_EQ(hashes1, read_hashes1);
  EXPECT_EQ(hashes2, read_hashes2);

  // Finally make sure that we can retrieve the hashes for the subdir
  // path even when that path contains forward slashes (on windows).
  base::FilePath path2_fwd_slashes =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  block_size = 0;
  EXPECT_TRUE(reader.GetHashes(path2_fwd_slashes, &block_size, &read_hashes2));
  EXPECT_EQ(hashes2, read_hashes2);
}

}  // namespace extensions
