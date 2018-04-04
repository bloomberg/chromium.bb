// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/services/media_gallery_util/public/mojom/constants.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/service_manager_connection.h"
#include "media/media_buildflags.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_FFMPEG)
extern "C" {
#include <libavutil/cpu.h>
}
#endif

namespace {

using MediaGalleryUtilBrowserTest = InProcessBrowserTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(MediaGalleryUtilBrowserTest, TestThirdPartyCpuInfo) {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  chrome::mojom::MediaParserPtr media_parser_ptr;
  connector->BindInterface(chrome::mojom::kMediaGalleryUtilServiceName,
                           mojo::MakeRequest(&media_parser_ptr));

  base::RunLoop run_loop;
  media_parser_ptr->GetCpuInfo(base::BindOnce(
      [](base::Closure quit_closure, int64_t libyuv_cpu_flags,
         int64_t ffmpeg_cpu_flags) {
        int64_t expected_ffmpeg_cpu_flags = 0;
#if BUILDFLAG(ENABLE_FFMPEG)
        expected_ffmpeg_cpu_flags = av_get_cpu_flags();
#endif
        EXPECT_EQ(expected_ffmpeg_cpu_flags, ffmpeg_cpu_flags);
        EXPECT_EQ(libyuv::InitCpuFlags(), libyuv_cpu_flags);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}
