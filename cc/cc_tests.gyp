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
      'FloatQuadTest.cpp',
    ],
    'cc_tests_support_files': [
      'test/CCAnimationTestCommon.cpp',
      'test/CCAnimationTestCommon.h',
      'test/CCGeometryTestUtils.cpp',
      'test/CCGeometryTestUtils.h',
      'test/CCLayerTestCommon.cpp',
      'test/CCLayerTestCommon.h',
      'test/CCOcclusionTrackerTestCommon.h',
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
      'test/WebCompositorInitializer.h',
    ],
  },
  'targets': [
    {
      'target_name': 'cc_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../testing/gmock.gyp:gmock',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['use_libcc_for_compositor==1', {
          'dependencies': [
            '../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            '../skia/skia.gyp:skia',
            'cc.gyp:cc',
            'cc_test_support',
          ],
          'defines': [
            'USE_LIBCC_FOR_COMPOSITOR',
            'WTF_USE_ACCELERATED_COMPOSITING=1',
          ],
          'include_dirs': [
            'stubs',
            'test',
            '.',
            '../third_party/WebKit/Source/Platform/chromium',
          ],
          'sources': [
            '<@(cc_tests_source_files)',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['use_libcc_for_compositor==1', {
      'targets': [
        {
          'target_name': 'cc_test_support',
          'type': 'static_library',
          'defines': [
            'WTF_USE_ACCELERATED_COMPOSITING=1',
          ],
          'include_dirs': [
            'stubs',
            'test',
            '.',
            '..',
            '../third_party/WebKit/Source/Platform/chromium',
          ],
          'dependencies': [
            '../ui/gl/gl.gyp:gl',
            '../testing/gtest.gyp:gtest',
            '../testing/gmock.gyp:gmock',
            '../skia/skia.gyp:skia',
            '../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit_wtf_support',
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
            '../webkit/compositor_bindings/compositor_bindings.gyp:webkit_compositor_support',
            '../webkit/support/webkit_support.gyp:glue',
          ],
          'sources': [
            '<@(cc_tests_support_files)',
          ],
        },
      ],
    }],
  ],
}

