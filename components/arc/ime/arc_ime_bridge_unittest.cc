// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"

namespace arc {

namespace {

class FakeArcImeIpcHost : public ArcImeIpcHost {
 public:
  void SendSetCompositionText(const ui::CompositionText& composition) override {
  }
  void SendConfirmCompositionText() override {
  }
  void SendInsertText(const base::string16& text) override {
  }
};

}  // namespace

class ArcImeBridgeTest : public testing::Test {
 public:
  ArcImeBridgeTest() {}

 protected:
  scoped_ptr<FakeArcBridgeService> fake_arc_bridge_service_;
  scoped_ptr<ArcImeBridge> instance_;

 private:
  void SetUp() override {
    fake_arc_bridge_service_.reset(new FakeArcBridgeService);
    instance_.reset(new ArcImeBridge(fake_arc_bridge_service_.get()));
    instance_->SetIpcHostForTesting(make_scoped_ptr(new FakeArcImeIpcHost));
  }

  void TearDown() override {
    instance_.reset();
    fake_arc_bridge_service_.reset();
  }
};

TEST_F(ArcImeBridgeTest, HasCompositionText) {
  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("nonempty text");

  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ClearCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ConfirmCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->InsertText(base::UTF8ToUTF16("another text"));
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->SetCompositionText(ui::CompositionText());
  EXPECT_FALSE(instance_->HasCompositionText());
}

}  // namespace arc
