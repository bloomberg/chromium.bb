// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/lww_register.h"

#include <string>

#include "base/macros.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/revision_generator.h"
#include "blimp/helium/version_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

class LwwRegisterTest : public HeliumTest {
 public:
  LwwRegisterTest() {}
  ~LwwRegisterTest() override = default;

 protected:
  void Initialize(Peer bias) {
    client_ = base::MakeUnique<LwwRegister<int>>(bias, Peer::CLIENT);
    engine_ = base::MakeUnique<LwwRegister<int>>(bias, Peer::ENGINE);
  }

  void SyncFromClient() {
    Sync(client_.get(), engine_.get(), engine_->GetVersionVector(),
         client_->GetVersionVector());
  }

  void SyncFromEngine() {
    Sync(engine_.get(), client_.get(), client_->GetVersionVector(),
         engine_->GetVersionVector());
  }

  void Sync(LwwRegister<int>* from_lww_register,
            LwwRegister<int>* to_lww_register,
            VersionVector from,
            VersionVector to);

  std::unique_ptr<LwwRegister<int>> client_;
  std::unique_ptr<LwwRegister<int>> engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LwwRegisterTest);
};

// Takes a changeset from |from_lww_register| and applies it to
// |to_lww_register|.
void LwwRegisterTest::Sync(LwwRegister<int>* from_lww_register,
                           LwwRegister<int>* to_lww_register,
                           VersionVector from,
                           VersionVector to) {
  // Create a changeset from |from_lww_register|.
  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  from_lww_register->CreateChangesetToCurrent(from.remote_revision(),
                                              &output_stream);

  // Apply the changeset to |to_lww_register|.
  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);
  to_lww_register->ApplyChangeset(to.local_revision(), &input_stream);
}

TEST_F(LwwRegisterTest, SetIncrementsLocalVersion) {
  Initialize(Peer::CLIENT);

  VersionVector earlier_version = client_->GetVersionVector();
  client_->Set(42);
  VersionVector current_version = client_->GetVersionVector();

  EXPECT_EQ(42, client_->Get());
  EXPECT_LT(earlier_version.local_revision(), current_version.local_revision());
}

TEST_F(LwwRegisterTest, ApplyLaterChangeset) {
  Initialize(Peer::CLIENT);

  client_->Set(123);
  SyncFromClient();

  EXPECT_EQ(123, engine_->Get());
}

TEST_F(LwwRegisterTest, ApplyEarlierChangeset) {
  Initialize(Peer::CLIENT);

  client_->Set(123);
  SyncFromClient();

  engine_->Set(456);
  SyncFromClient();

  EXPECT_EQ(456, engine_->Get());
}

TEST_F(LwwRegisterTest, ClientApplyChangesetConflictClientWins) {
  Initialize(Peer::CLIENT);

  client_->Set(123);
  engine_->Set(456);
  SyncFromEngine();

  EXPECT_EQ(123, client_->Get());
}

TEST_F(LwwRegisterTest, EngineApplyChangesetConflictClientWins) {
  Initialize(Peer::CLIENT);

  client_->Set(123);
  engine_->Set(456);
  SyncFromClient();

  EXPECT_EQ(123, engine_->Get());
}

TEST_F(LwwRegisterTest, ClientApplyChangesetConflictEngineWins) {
  Initialize(Peer::ENGINE);

  client_->Set(123);
  engine_->Set(456);
  SyncFromEngine();

  EXPECT_EQ(456, client_->Get());
}

TEST_F(LwwRegisterTest, EngineApplyChangesetConflictEngineWins) {
  Initialize(Peer::ENGINE);

  client_->Set(123);
  engine_->Set(456);
  SyncFromClient();

  EXPECT_EQ(456, engine_->Get());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
