// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher_impl.h"

#include <set>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service_impl.h"
#include "components/favicon/core/favicon_util.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/image_fetcher/image_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnArg;

namespace ntp_tiles {
namespace {

class MockImageFetcher : public image_fetcher::ImageFetcher {
 public:
  MOCK_METHOD1(SetImageFetcherDelegate,
               void(image_fetcher::ImageFetcherDelegate* delegate));
  MOCK_METHOD1(SetDataUseServiceName,
               void(image_fetcher::ImageFetcher::DataUseServiceName name));
  MOCK_METHOD3(StartOrQueueNetworkRequest,
               void(const std::string& id,
                    const GURL& image_url,
                    base::Callback<void(const std::string& id,
                                        const gfx::Image& image)> callback));
  MOCK_METHOD1(SetDesiredImageFrameSize, void(const gfx::Size&));
};

// This class provides methods to inject an image resource where a real resource
// would be necessary otherwise. All other methods have return values that allow
// the normal implementation to proceed.
class MockResourceDelegate : public ui::ResourceBundle::Delegate {
 public:
  ~MockResourceDelegate() override {}

  MOCK_METHOD1(GetImageNamed, gfx::Image(int resource_id));
  MOCK_METHOD1(GetNativeImageNamed, gfx::Image(int resource_id));

  MOCK_METHOD2(GetPathForResourcePack,
               base::FilePath(const base::FilePath& pack_path,
                              ui::ScaleFactor scale_factor));

  MOCK_METHOD2(GetPathForLocalePack,
               base::FilePath(const base::FilePath& pack_path,
                              const std::string& locale));

  MOCK_METHOD2(LoadDataResourceBytes,
               base::RefCountedMemory*(int resource_id,
                                       ui::ScaleFactor scale_factor));

  MOCK_METHOD3(GetRawDataResource,
               bool(int resource_id,
                    ui::ScaleFactor scale_factor,
                    base::StringPiece* value));

  MOCK_METHOD2(GetLocalizedString, bool(int message_id, base::string16* value));
};

class IconCacherTest : public ::testing::Test {
 protected:
  IconCacherTest()
      : site_(base::string16(),  // title, unused
              GURL("http://url.google/"),
              GURL("http://url.google/icon.png"),
              GURL("http://url.google/favicon.ico"),
              GURL()),  // thumbnail, unused
        image_fetcher_(new ::testing::StrictMock<MockImageFetcher>),
        favicon_service_(/*favicon_client=*/nullptr, &history_service_),
        task_runner_(new base::TestSimpleTaskRunner()) {
    CHECK(history_dir_.CreateUniqueTempDir());
    CHECK(history_service_.Init(
        history::HistoryDatabaseParams(history_dir_.GetPath(), 0, 0)));
  }

  void SetUp() override {
    if (ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::CleanupSharedInstance();
    }
    ON_CALL(mock_resource_delegate_, GetPathForResourcePack(_, _))
        .WillByDefault(ReturnArg<0>());
    ON_CALL(mock_resource_delegate_, GetPathForLocalePack(_, _))
        .WillByDefault(ReturnArg<0>());
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  }

  void TearDown() override {
    if (ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::CleanupSharedInstance();
    }
    base::FilePath pak_path;
#if defined(OS_ANDROID)
    PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    PathService::Get(base::DIR_MODULE, &pak_path);
#endif

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::SCALE_FACTOR_NONE);
  }

  void PreloadIcon(const GURL& url,
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   int width,
                   int height) {
    favicon_service_.SetFavicons(url, icon_url, icon_type,
                                 gfx::test::CreateImage(width, height));
  }

  bool IconIsCachedFor(const GURL& url, favicon_base::IconType icon_type) {
    return !GetCachedIconFor(url, icon_type).IsEmpty();
  }

  gfx::Image GetCachedIconFor(const GURL& url,
                              favicon_base::IconType icon_type) {
    base::CancelableTaskTracker tracker;
    gfx::Image image;
    base::RunLoop loop;
    favicon::GetFaviconImageForPageURL(
        &favicon_service_, url, icon_type,
        base::Bind(
            [](gfx::Image* image, base::RunLoop* loop,
               const favicon_base::FaviconImageResult& result) {
              *image = result.image;
              loop->Quit();
            },
            &image, &loop),
        &tracker);
    loop.Run();
    return image;
  }

  void WaitForTasksToFinish() { task_runner_->RunUntilIdle(); }

  base::MessageLoop message_loop_;
  PopularSites::Site site_;
  std::unique_ptr<MockImageFetcher> image_fetcher_;
  base::ScopedTempDir history_dir_;
  history::HistoryService history_service_;
  favicon::FaviconServiceImpl favicon_service_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  NiceMock<MockResourceDelegate> mock_resource_delegate_;
};

ACTION(FailFetch) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(arg2, arg0, gfx::Image()));
}

ACTION_P2(PassFetch, width, height) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(arg2, arg0, gfx::test::CreateImage(width, height)));
}

ACTION_P(Quit, run_loop) {
  run_loop->Quit();
}

TEST_F(IconCacherTest, LargeCached) {
  base::MockCallback<base::Closure> done;
  EXPECT_CALL(done, Run()).Times(0);
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_, SetDesiredImageFrameSize(gfx::Size(128, 128)));
  }
  PreloadIcon(site_.url, site_.large_icon_url, favicon_base::TOUCH_ICON, 128,
              128);
  IconCacherImpl cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, done.Get(), done.Get());
  WaitForTasksToFinish();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, LargeNotCachedAndFetchSucceeded) {
  base::MockCallback<base::Closure> done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_, SetDesiredImageFrameSize(gfx::Size(128, 128)));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.large_icon_url, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Run()).WillOnce(Quit(&loop));
  }

  IconCacherImpl cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, done.Get(), done.Get());
  loop.Run();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, SmallNotCachedAndFetchSucceeded) {
  site_.large_icon_url = GURL();

  base::MockCallback<base::Closure> done;
  base::RunLoop loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_, SetDesiredImageFrameSize(gfx::Size(128, 128)));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.favicon_url, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(done, Run()).WillOnce(Quit(&loop));
  }

  IconCacherImpl cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, done.Get(), done.Get());
  loop.Run();
  EXPECT_TRUE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, LargeNotCachedAndFetchFailed) {
  base::MockCallback<base::Closure> done;
  EXPECT_CALL(done, Run()).Times(0);
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_, SetDesiredImageFrameSize(gfx::Size(128, 128)));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.large_icon_url, _))
        .WillOnce(FailFetch());
  }

  IconCacherImpl cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, done.Get(), done.Get());
  WaitForTasksToFinish();
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::FAVICON));
  EXPECT_FALSE(IconIsCachedFor(site_.url, favicon_base::TOUCH_ICON));
}

TEST_F(IconCacherTest, HandlesEmptyCallbacksNicely) {
  EXPECT_CALL(*image_fetcher_, SetDataUseServiceName(_));
  EXPECT_CALL(*image_fetcher_, SetDesiredImageFrameSize(_));
  ON_CALL(*image_fetcher_, StartOrQueueNetworkRequest(_, _, _))
      .WillByDefault(PassFetch(128, 128));
  IconCacherImpl cacher(&favicon_service_, std::move(image_fetcher_));
  cacher.StartFetch(site_, base::Closure(), base::Closure());
  WaitForTasksToFinish();
}

TEST_F(IconCacherTest, ProvidesDefaultIconAndSucceedsWithFetching) {
  // We are not interested which delegate function actually handles the call to
  // |GetNativeImageNamed| as long as we receive the right image.
  ON_CALL(mock_resource_delegate_, GetNativeImageNamed(12345))
      .WillByDefault(Return(gfx::test::CreateImage(64, 64)));
  ON_CALL(mock_resource_delegate_, GetImageNamed(12345))
      .WillByDefault(Return(gfx::test::CreateImage(64, 64)));
  base::MockCallback<base::Closure> preliminary_icon_available;
  base::MockCallback<base::Closure> icon_available;
  base::RunLoop default_loop;
  base::RunLoop fetch_loop;
  {
    InSequence s;
    EXPECT_CALL(*image_fetcher_,
                SetDataUseServiceName(
                    data_use_measurement::DataUseUserData::NTP_TILES));
    EXPECT_CALL(*image_fetcher_, SetDesiredImageFrameSize(gfx::Size(128, 128)));
    EXPECT_CALL(preliminary_icon_available, Run())
        .WillOnce(Quit(&default_loop));
    EXPECT_CALL(*image_fetcher_,
                StartOrQueueNetworkRequest(_, site_.large_icon_url, _))
        .WillOnce(PassFetch(128, 128));
    EXPECT_CALL(icon_available, Run()).WillOnce(Quit(&fetch_loop));
  }

  IconCacherImpl cacher(&favicon_service_, std::move(image_fetcher_));
  site_.default_icon_resource = 12345;
  cacher.StartFetch(site_, icon_available.Get(),
                    preliminary_icon_available.Get());

  default_loop.Run();  // Wait for the default image.
  EXPECT_THAT(GetCachedIconFor(site_.url, favicon_base::TOUCH_ICON).Size(),
              Eq(gfx::Size(64, 64)));  // Compares dimensions, not objects.

  // Let the fetcher continue and wait for the second call of the callback.
  fetch_loop.Run();  // Wait for the updated image.
  EXPECT_THAT(GetCachedIconFor(site_.url, favicon_base::TOUCH_ICON).Size(),
              Eq(gfx::Size(128, 128)));  // Compares dimensions, not objects.
}

}  // namespace
}  // namespace ntp_tiles
