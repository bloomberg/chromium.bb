# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'use_libcc_for_compositor%': 0,
    'cc_tests_source_files': [
      'CCActiveAnimationTest.cpp',
      'CCDamageTrackerTest.cpp',
      'CCDelayBasedTimeSourceTest.cpp',
      'CCFrameRateControllerTest.cpp',
      'CCHeadsUpDisplayTest.cpp',
      'CCKeyframedAnimationCurveTest.cpp',
      'CCLayerAnimationControllerTest.cpp',
      'CCLayerImplTest.cpp',
      'CCLayerIteratorTest.cpp',
      'CCLayerQuadTest.cpp',
      'CCLayerSorterTest.cpp',
      'CCLayerTreeHostCommonTest.cpp',
      'CCLayerTreeHostImplTest.cpp',
      'CCLayerTreeHostTest.cpp',
      'CCMathUtilTest.cpp',
      'CCOcclusionTrackerTest.cpp',
      'CCPrioritizedTextureTest.cpp',
      'CCQuadCullerTest.cpp',
      'CCRenderSurfaceFiltersTest.cpp',
      'CCRenderSurfaceTest.cpp',
      'CCResourceProviderTest.cpp',
      'CCSchedulerStateMachineTest.cpp',
      'CCSchedulerTest.cpp',
      'CCSchedulerTest.cpp',
      'CCScopedTextureTest.cpp',
      'CCScrollbarAnimationControllerLinearFadeTest.cpp',
      'CCSolidColorLayerImplTest.cpp',
      'CCTextureUpdateControllerTest.cpp',
      'CCThreadTaskTest.cpp',
      'CCThreadedTest.cpp',
      'CCThreadedTest.h',
      'CCTiledLayerImplTest.cpp',
      'CCTimerTest.cpp',
      'test/CCAnimationTestCommon.cpp',
      'test/CCAnimationTestCommon.h',
      'test/CCLayerTestCommon.cpp',
      'test/CCLayerTestCommon.h',
      'test/CCLayerTreeTestCommon.h',
      'test/CCLayerTreeTestCommon.h',
      'test/CCOcclusionTrackerTestCommon.h',
      'test/CCSchedulerTestCommon.h',
      'test/CCSchedulerTestCommon.h',
      'test/CCTestCommon.h',
      'test/CCTiledLayerTestCommon.cpp',
      'test/CCTiledLayerTestCommon.h',
      'test/CompositorFakeWebGraphicsContext3D.h',
      'test/FakeCCGraphicsContext.h',
      'test/FakeCCLayerTreeHostClient.h',
      'test/FakeGraphicsContext3DTest.cpp',
      'test/FakeWebCompositorOutputSurface.h',
      'test/FakeWebGraphicsContext3D.h',
      'test/FakeWebScrollbarThemeGeometry.h',
      'test/MockCCQuadCuller.h',
    ]
  },
  'conditions': [
    ['use_libcc_for_compositor==1 and component!="shared_library"', {
      'targets': [
        {
          'target_name': 'cc_unittests',
          'type': 'executable',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_support',
            '<(DEPTH)/skia/skia.gyp:skia',
            # We have to depend on WTF directly to pick up the correct defines for WTF headers - for instance USE_SYSTEM_MALLOC.
            '<(DEPTH)/third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            '<(DEPTH)/third_party/WebKit/Source/Platform/Platform.gyp/Platform.gyp:webkit_platform',
            'cc.gyp:cc',
          ],
          'defines': [
            'WTF_USE_ACCELERATED_COMPOSITING=1',
          ],
          'include_dirs': [
            'stubs',
            'test',
            '.',
          ],
          'sources': [
            '<@(cc_tests_source_files)',
            'test/run_all_unittests.cc',
          ],
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'cc_unittests',
          'type': 'none',
        }
      ]
    }],
  ],
}

