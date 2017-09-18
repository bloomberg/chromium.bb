// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/file_helper.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"

namespace exo {
namespace {

enum class DragEvent { kOffer, kEnter, kLeave, kMotion, kDrop, kDestroy };

class TestDataOfferDelegate : public DataOfferDelegate {
 public:
  ~TestDataOfferDelegate() override {}

  // Overridden from DataOfferDelegate:
  void OnDataOfferDestroying(DataOffer* offer) override { delete this; }
  void OnOffer(const std::string& mime_type) override {}
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {}
  void OnAction(DndAction action) override {}
};

class TestDataDeviceDelegate : public DataDeviceDelegate {
 public:
  TestDataDeviceDelegate() {}

  size_t PopEvents(std::vector<DragEvent>* out) {
    out->swap(events_);
    events_.clear();
    return out->size();
  }
  Surface* entered_surface() const { return entered_surface_; }
  void DeleteDataOffer() { data_offer_.reset(); }
  void set_can_accept_data_events_for_surface(bool value) {
    can_accept_data_events_for_surface_ = value;
  }

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* data_device) override {
    events_.push_back(DragEvent::kDestroy);
  }
  DataOffer* OnDataOffer() override {
    events_.push_back(DragEvent::kOffer);
    data_offer_.reset(new DataOffer(new TestDataOfferDelegate));
    return data_offer_.get();
  }
  void OnEnter(Surface* surface,
               const gfx::PointF& location,
               const DataOffer& data_offer) override {
    events_.push_back(DragEvent::kEnter);
    entered_surface_ = surface;
  }
  void OnLeave() override { events_.push_back(DragEvent::kLeave); }
  void OnMotion(base::TimeTicks time_stamp,
                const gfx::PointF& location) override {
    events_.push_back(DragEvent::kMotion);
  }
  void OnDrop() override { events_.push_back(DragEvent::kDrop); }
  void OnSelection(const DataOffer& data_offer) override {}
  bool CanAcceptDataEventsForSurface(Surface* surface) override {
    return can_accept_data_events_for_surface_;
  }

 private:
  std::vector<DragEvent> events_;
  std::unique_ptr<DataOffer> data_offer_;
  Surface* entered_surface_ = nullptr;
  bool can_accept_data_events_for_surface_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestDataDeviceDelegate);
};

class TestFileHelper : public FileHelper {
 public:
  TestFileHelper() = default;

  // Overridden from FileHelper:
  std::string GetMimeTypeForUriList() const override { return ""; }
  bool ConvertPathToUrl(const base::FilePath& path, GURL* out) override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFileHelper);
};

class DataDeviceTest : public test::ExoTestBase {
 public:
  void SetUp() override {
    test::ExoTestBase::SetUp();
    device_ = std::make_unique<DataDevice>(&delegate_, &file_helper_);
    data_.SetString(base::string16(base::ASCIIToUTF16("Test data")));
    surface_ = std::make_unique<Surface>();
  }

  void TearDown() override {
    surface_.reset();
    device_.reset();
    test::ExoTestBase::TearDown();
  }

 protected:
  TestDataDeviceDelegate delegate_;
  TestFileHelper file_helper_;
  std::unique_ptr<DataDevice> device_;
  ui::OSExchangeData data_;
  std::unique_ptr<Surface> surface_;
};

TEST_F(DataDeviceTest, Destroy) {
  std::vector<DragEvent> events;
  device_.reset();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kDestroy, events[0]);
}

TEST_F(DataDeviceTest, DragEventsDrop) {
  ui::DropTargetEvent event(data_, gfx::Point(), gfx::Point(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DragEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kOffer, events[0]);
  EXPECT_EQ(DragEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK, device_->OnDragUpdated(event));
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kMotion, events[0]);

  device_->OnPerformDrop(event);
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kDrop, events[0]);
}

TEST_F(DataDeviceTest, DragEventsExit) {
  ui::DropTargetEvent event(data_, gfx::Point(), gfx::Point(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DragEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kOffer, events[0]);
  EXPECT_EQ(DragEvent::kEnter, events[1]);

  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK, device_->OnDragUpdated(event));
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kMotion, events[0]);

  device_->OnDragExited();
  ASSERT_EQ(1u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kLeave, events[0]);
}

TEST_F(DataDeviceTest, DeleteDataOfferDuringDrag) {
  ui::DropTargetEvent event(data_, gfx::Point(), gfx::Point(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DragEvent> events;
  device_->OnDragEntered(event);
  ASSERT_EQ(2u, delegate_.PopEvents(&events));
  EXPECT_EQ(DragEvent::kOffer, events[0]);
  EXPECT_EQ(DragEvent::kEnter, events[1]);

  delegate_.DeleteDataOffer();

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, device_->OnDragUpdated(event));
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  device_->OnPerformDrop(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

TEST_F(DataDeviceTest, NotAcceptDataEventsForSurface) {
  ui::DropTargetEvent event(data_, gfx::Point(), gfx::Point(),
                            ui::DragDropTypes::DRAG_MOVE);
  ui::Event::DispatcherApi(&event).set_target(surface_->window());

  std::vector<DragEvent> events;
  delegate_.set_can_accept_data_events_for_surface(false);

  device_->OnDragEntered(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, device_->OnDragUpdated(event));
  EXPECT_EQ(0u, delegate_.PopEvents(&events));

  device_->OnPerformDrop(event);
  EXPECT_EQ(0u, delegate_.PopEvents(&events));
}

}  // namespace
}  // namespace exo
