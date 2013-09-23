// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/streams.h"

class EnsembleTest : public BaseTest {
 public:

  void TestEnsemble(std::string src_bytes, std::string tgt_bytes) const;

  void PeEnsemble() const;
  void Pe64Ensemble() const;
  void Elf32Ensemble() const;
};

void EnsembleTest::TestEnsemble(std::string src_bytes,
                                std::string tgt_bytes) const {

  courgette::SourceStream source;
  courgette::SourceStream target;

  source.Init(src_bytes);
  target.Init(tgt_bytes);

  courgette::SinkStream patch_sink;

  courgette::Status status;

  status = courgette::GenerateEnsemblePatch(&source, &target, &patch_sink);
  EXPECT_EQ(courgette::C_OK, status);

  courgette::SourceStream patch_source;
  patch_source.Init(patch_sink.Buffer(), patch_sink.Length());

  courgette::SinkStream patch_result;

  status = courgette::ApplyEnsemblePatch(&source, &patch_source, &patch_result);
  EXPECT_EQ(courgette::C_OK, status);

  EXPECT_EQ(target.OriginalLength(), patch_result.Length());
  EXPECT_FALSE(memcmp(target.Buffer(),
                      patch_result.Buffer(),
                      target.OriginalLength()));
}

void EnsembleTest::Elf32Ensemble() const {
  std::list<std::string> src_ensemble;
  std::list<std::string> tgt_ensemble;

  src_ensemble.push_back("elf-32-1");

  tgt_ensemble.push_back("elf-32-2");

  std::string src_bytes = FilesContents(src_ensemble);
  std::string tgt_bytes = FilesContents(tgt_ensemble);

  src_bytes = "aaabbbccc" + src_bytes + "dddeeefff";
  tgt_bytes = "aaagggccc" + tgt_bytes + "dddeeefff";

  TestEnsemble(src_bytes, tgt_bytes);
}

void EnsembleTest::PeEnsemble() const {
  std::list<std::string> src_ensemble;
  std::list<std::string> tgt_ensemble;

  src_ensemble.push_back("en-US.dll");
  src_ensemble.push_back("setup1.exe");

  tgt_ensemble.push_back("en-US.dll");
  tgt_ensemble.push_back("setup2.exe");

  std::string src_bytes = FilesContents(src_ensemble);
  std::string tgt_bytes = FilesContents(tgt_ensemble);

  src_bytes = "aaabbbccc" + src_bytes + "dddeeefff";
  tgt_bytes = "aaagggccc" + tgt_bytes + "dddeeefff";

  TestEnsemble(src_bytes, tgt_bytes);
}

void EnsembleTest::Pe64Ensemble() const {
  std::list<std::string> src_ensemble;
  std::list<std::string> tgt_ensemble;

  src_ensemble.push_back("en-US-64.dll");
  src_ensemble.push_back("chrome64_1.exe");
  src_ensemble.push_back("pe-64.exe");

  tgt_ensemble.push_back("en-US-64.dll");
  tgt_ensemble.push_back("chrome64_2.exe");
  tgt_ensemble.push_back("pe-64.exe");

  std::string src_bytes = FilesContents(src_ensemble);
  std::string tgt_bytes = FilesContents(tgt_ensemble);

  src_bytes = "aaabbbccc" + src_bytes + "dddeeefff";
  tgt_bytes = "aaagggccc" + tgt_bytes + "dddeeefff";

  TestEnsemble(src_bytes, tgt_bytes);
}

// Ensemble tests still take too long on Windows so disabling for now
// TODO(dgarrett) http://code.google.com/p/chromium/issues/detail?id=101614

TEST_F(EnsembleTest, DISABLED_PE) {
  PeEnsemble();
}

TEST_F(EnsembleTest, DISABLED_PE64) {
  Pe64Ensemble();
}

TEST_F(EnsembleTest, DISABLED_Elf32) {
  Elf32Ensemble();
}
