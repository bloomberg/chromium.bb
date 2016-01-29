// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/combined_desktop_media_list.h"

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_media_list_base.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

using testing::DoAll;
using content::DesktopMediaID;

static const int kThumbnailSize = 50;
static const int kDefaultSourceCount = 2;

// Create a greyscale image with certain size and grayscale value.
gfx::ImageSkia CreateGrayscaleImage(gfx::Size size, uint8_t greyscale_value) {
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height(), true);

  result.lockPixels();
  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());

  // Set greyscale value for all pixels.
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel()] =
          greyscale_value;
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 1] =
          greyscale_value;
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 2] =
          greyscale_value;
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3] =
          0xff;
    }
  }

  result.unlockPixels();

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

// Fake Implementation of DesktopMediaListBase.
class FakeDesktopMediaListBaseImpl : public DesktopMediaListBase {
 public:
  explicit FakeDesktopMediaListBaseImpl(DesktopMediaID::Type type)
      : DesktopMediaListBase(base::TimeDelta::FromMilliseconds(1)),
        media_type_(type) {
    SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    for (int i = 0; i < kDefaultSourceCount; ++i)
      AddFakeSource(i, base::UTF8ToUTF16("Test media"), i);
  }

  ~FakeDesktopMediaListBaseImpl() override {}

  void AddFakeSource(int index, base::string16 title, int greyscale_value) {
    DesktopMediaID id(media_type_, index + 1);
    fake_sources_.push_back(DesktopMediaListBase::SourceDescription(id, title));
    fake_thumbnails_.push_back(
        CreateGrayscaleImage(gfx::Size(10, 10), greyscale_value));
    current_thumbnail_map_[id.id] = greyscale_value;
  }

  void RemoveFakeSource(int index) {
    if (static_cast<size_t>(index) >= fake_sources_.size())
      return;

    current_thumbnail_map_.erase(fake_sources_[index].id.id);
    fake_sources_.erase(fake_sources_.begin() + index);
    fake_thumbnails_.erase(fake_thumbnails_.begin() + index);
  }

 private:
  void Refresh() override {
    UpdateSourcesList(fake_sources_);

    // Update thumbnails.
    for (size_t i = 0; i < fake_sources_.size(); i++) {
      // only update when a thumbnail is added or changed.
      const int id = fake_sources_[i].id.id;
      if (!refreshed_thumbnail_map_.count(id) ||
          (refreshed_thumbnail_map_[id] != current_thumbnail_map_[id])) {
        UpdateSourceThumbnail(fake_sources_[i].id, fake_thumbnails_[i]);
      }
    }
    refreshed_thumbnail_map_ = current_thumbnail_map_;

    ScheduleNextRefresh();
  }

  std::vector<DesktopMediaListBase::SourceDescription> fake_sources_;
  std::vector<gfx::ImageSkia> fake_thumbnails_;
  DesktopMediaID::Type media_type_;
  // The current and last refrehed maps of source id and thumbnail's greyscale.
  // They are used for detect the thumbnail add or change.
  std::map<int, int> current_thumbnail_map_, refreshed_thumbnail_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaListBaseImpl);
};

class MockObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD2(OnSourceAdded, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceRemoved, void(DesktopMediaList* list, int index));
  MOCK_METHOD3(OnSourceMoved,
               void(DesktopMediaList* list, int old_index, int new_index));
  MOCK_METHOD2(OnSourceNameChanged, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceThumbnailChanged,
               void(DesktopMediaList* list, int index));

  void VerifyAndClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(this);
  }
};

ACTION_P2(CheckListSize, list, expected_list_size) {
  EXPECT_EQ(expected_list_size, list->GetSourceCount());
}

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

class CombinedDesktopMediaListTest : public testing::Test {
 public:
  CombinedDesktopMediaListTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
    list1_ = new FakeDesktopMediaListBaseImpl(DesktopMediaID::TYPE_SCREEN);
    list2_ = new FakeDesktopMediaListBaseImpl(DesktopMediaID::TYPE_WINDOW);

    scoped_ptr<DesktopMediaList> list1(list1_);
    scoped_ptr<DesktopMediaList> list2(list2_);

    std::vector<scoped_ptr<DesktopMediaList>> lists;
    lists.push_back(std::move(list1));
    lists.push_back(std::move(list2));

    combined_list_.reset(new CombinedDesktopMediaList(lists));
  }

  // StartUpdating() and verify the first call of refresh().
  void InitializeAndVerify() {
    {
      testing::InSequence dummy;

      // list1_'s refresh.
      for (int i = 0; i < kDefaultSourceCount; ++i) {
        EXPECT_CALL(observer_, OnSourceAdded(combined_list_.get(), i))
            .WillOnce(CheckListSize(combined_list_.get(), i + 1));
      }

      for (int i = 0; i < kDefaultSourceCount; ++i) {
        EXPECT_CALL(observer_,
                    OnSourceThumbnailChanged(combined_list_.get(), i));
      }

      // list2_'s refresh.
      for (int i = kDefaultSourceCount; i < 2 * kDefaultSourceCount; ++i) {
        EXPECT_CALL(observer_, OnSourceAdded(combined_list_.get(), i))
            .WillOnce(CheckListSize(combined_list_.get(), i + 1));
      }

      for (int i = kDefaultSourceCount; i < 2 * kDefaultSourceCount - 1; ++i) {
        EXPECT_CALL(observer_,
                    OnSourceThumbnailChanged(combined_list_.get(), i));
      }

      EXPECT_CALL(observer_,
                  OnSourceThumbnailChanged(combined_list_.get(),
                                           2 * kDefaultSourceCount - 1))
          .WillOnce(QuitMessageLoop(&message_loop_));
    }

    combined_list_->StartUpdating(&observer_);
    message_loop_.Run();

    // list1_'s sources.
    for (int i = 0; i < kDefaultSourceCount; ++i) {
      EXPECT_EQ(combined_list_->GetSource(i).id.type,
                content::DesktopMediaID::TYPE_SCREEN);
      EXPECT_EQ(combined_list_->GetSource(i).id.id, i + 1);
    }

    // list2_'s sources.
    for (int i = kDefaultSourceCount; i < 2 * kDefaultSourceCount; i++) {
      EXPECT_EQ(combined_list_->GetSource(i).id.type,
                content::DesktopMediaID::TYPE_WINDOW);
      EXPECT_EQ(combined_list_->GetSource(i).id.id,
                i - kDefaultSourceCount + 1);
    }

    observer_.VerifyAndClearExpectations();
  }

 protected:
  // Must be listed before |combined_list_|, so it's destroyed last.
  MockObserver observer_;
  FakeDesktopMediaListBaseImpl* list1_;
  FakeDesktopMediaListBaseImpl* list2_;
  scoped_ptr<CombinedDesktopMediaList> combined_list_;

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(CombinedDesktopMediaListTest);
};

TEST_F(CombinedDesktopMediaListTest, AddSource) {
  InitializeAndVerify();

  int index = kDefaultSourceCount;
  list1_->AddFakeSource(index, base::UTF8ToUTF16("Test media"), index);

  EXPECT_CALL(observer_, OnSourceAdded(combined_list_.get(), index))
      .WillOnce(
          CheckListSize(combined_list_.get(), 2 * kDefaultSourceCount + 1));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(combined_list_.get(), index))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();

  list2_->AddFakeSource(index, base::UTF8ToUTF16("Test media"), index);

  EXPECT_CALL(observer_,
              OnSourceAdded(combined_list_.get(), 2 * kDefaultSourceCount + 1))
      .WillOnce(
          CheckListSize(combined_list_.get(), 2 * kDefaultSourceCount + 2));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(combined_list_.get(),
                                                  2 * kDefaultSourceCount + 1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();

  // Verify last source for list1_ and first source for list2_.
  EXPECT_EQ(combined_list_->GetSource(index).id.type,
            content::DesktopMediaID::TYPE_SCREEN);
  EXPECT_EQ(combined_list_->GetSource(index).id.id, index + 1);
  EXPECT_EQ(combined_list_->GetSource(index + 1).id.type,
            content::DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(combined_list_->GetSource(index + 1).id.id, 1);
}

TEST_F(CombinedDesktopMediaListTest, RemoveSource) {
  InitializeAndVerify();

  int index = kDefaultSourceCount - 1;
  list1_->RemoveFakeSource(index);

  EXPECT_CALL(observer_, OnSourceRemoved(combined_list_.get(), index))
      .WillOnce(DoAll(
          CheckListSize(combined_list_.get(), 2 * kDefaultSourceCount - 1),
          QuitMessageLoop(&message_loop_)));

  message_loop_.Run();

  list2_->RemoveFakeSource(index);

  EXPECT_CALL(observer_, OnSourceRemoved(combined_list_.get(),
                                         2 * kDefaultSourceCount - 2))
      .WillOnce(DoAll(
          CheckListSize(combined_list_.get(), 2 * kDefaultSourceCount - 2),
          QuitMessageLoop(&message_loop_)));

  message_loop_.Run();

  // Verify last source for list1_ and first source for list2_.
  EXPECT_EQ(combined_list_->GetSource(index - 1).id.type,
            content::DesktopMediaID::TYPE_SCREEN);
  EXPECT_EQ(combined_list_->GetSource(index - 1).id.id, index);
  EXPECT_EQ(combined_list_->GetSource(index).id.type,
            content::DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(combined_list_->GetSource(index).id.id, 1);
}

TEST_F(CombinedDesktopMediaListTest, MoveSource) {
  InitializeAndVerify();

  // Swap sources.
  list1_->RemoveFakeSource(kDefaultSourceCount - 1);
  list1_->RemoveFakeSource(kDefaultSourceCount - 2);
  list1_->AddFakeSource(kDefaultSourceCount - 1,
                        base::UTF8ToUTF16("Test media"),
                        kDefaultSourceCount - 1);
  list1_->AddFakeSource(kDefaultSourceCount - 2,
                        base::UTF8ToUTF16("Test media"),
                        kDefaultSourceCount - 2);

  EXPECT_CALL(observer_,
              OnSourceMoved(combined_list_.get(), kDefaultSourceCount - 1,
                            kDefaultSourceCount - 2))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();

  // Swap sources.
  list2_->RemoveFakeSource(kDefaultSourceCount - 1);
  list2_->RemoveFakeSource(kDefaultSourceCount - 2);
  list2_->AddFakeSource(kDefaultSourceCount - 1,
                        base::UTF8ToUTF16("Test media"),
                        kDefaultSourceCount - 1);
  list2_->AddFakeSource(kDefaultSourceCount - 2,
                        base::UTF8ToUTF16("Test media"),
                        kDefaultSourceCount - 2);

  EXPECT_CALL(observer_,
              OnSourceMoved(combined_list_.get(), 2 * kDefaultSourceCount - 1,
                            2 * kDefaultSourceCount - 2))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();
}

TEST_F(CombinedDesktopMediaListTest, UpdateTitle) {
  InitializeAndVerify();

  // Change title.
  list1_->RemoveFakeSource(kDefaultSourceCount - 1);
  list1_->AddFakeSource(kDefaultSourceCount - 1,
                        base::UTF8ToUTF16("New test media"),
                        kDefaultSourceCount - 1);

  EXPECT_CALL(observer_, OnSourceNameChanged(combined_list_.get(),
                                             kDefaultSourceCount - 1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();

  // Change title.
  list2_->RemoveFakeSource(kDefaultSourceCount - 1);
  list2_->AddFakeSource(kDefaultSourceCount - 1,
                        base::UTF8ToUTF16("New test media"),
                        kDefaultSourceCount - 1);

  EXPECT_CALL(observer_, OnSourceNameChanged(combined_list_.get(),
                                             2 * kDefaultSourceCount - 1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();

  EXPECT_EQ(combined_list_->GetSource(kDefaultSourceCount - 1).name,
            base::UTF8ToUTF16("New test media"));
  EXPECT_EQ(combined_list_->GetSource(2 * kDefaultSourceCount - 1).name,
            base::UTF8ToUTF16("New test media"));
}

TEST_F(CombinedDesktopMediaListTest, UpdateThumbnail) {
  InitializeAndVerify();

  // Change thumbnail.
  list1_->RemoveFakeSource(kDefaultSourceCount - 1);
  list1_->AddFakeSource(kDefaultSourceCount - 1,
                        base::UTF8ToUTF16("Test media"), 100);

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(combined_list_.get(),
                                                  kDefaultSourceCount - 1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();

  // Change thumbnail.
  list2_->RemoveFakeSource(kDefaultSourceCount - 1);
  list2_->AddFakeSource(kDefaultSourceCount - 1,
                        base::UTF8ToUTF16("Test media"), 100);

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(combined_list_.get(),
                                                  2 * kDefaultSourceCount - 1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  message_loop_.Run();
}
