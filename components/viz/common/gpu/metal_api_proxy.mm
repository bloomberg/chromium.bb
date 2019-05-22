// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_api_proxy.h"

#include "ui/gl/progress_reporter.h"

@implementation MTLDeviceProxy
- (id)initWithDevice:(id<MTLDevice>)device {
  if (self = [super init])
    device_.reset(device, base::scoped_policy::RETAIN);
  return self;
}

- (void)setProgressReporter:(gl::ProgressReporter*)progressReporter {
  progressReporter_ = progressReporter;
}

// Wrappers that add a gl::ScopedProgressReporter around calls to the true
// MTLDevice. For a given method, the method name is fn, return type is R, the
// argument types are A0,A1,A2,A3, and the argument names are a0,a1,a2,a3.
#define PROXY_METHOD0(R, fn) \
  -(R)fn {                   \
    return [device_ fn];     \
  }
#define PROXY_METHOD1(R, fn, A0) \
  -(R)fn : (A0)a0 {              \
    return [device_ fn:a0];      \
  }
#define PROXY_METHOD2(R, fn, A0, a1, A1) \
  -(R)fn : (A0)a0 a1 : (A1)a1 {          \
    return [device_ fn:a0 a1:a1];        \
  }
#define PROXY_METHOD3(R, fn, A0, a1, A1, a2, A2) \
  -(R)fn : (A0)a0 a1 : (A1)a1 : (A2)a2 {          \
    return [device_ fn:a0 a1:a1 a2:a2];        \
  }
#define PROXY_METHOD0_SLOW(R, fn)                                  \
  -(R)fn {                                                         \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn];                                           \
  }
#define PROXY_METHOD1_SLOW(R, fn, A0)                              \
  -(R)fn : (A0)a0 {                                                \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0];                                        \
  }
#define PROXY_METHOD2_SLOW(R, fn, A0, a1, A1)                      \
  -(R)fn : (A0)a0 a1 : (A1)a1 {                                    \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0 a1:a1];                                  \
  }
#define PROXY_METHOD3_SLOW(R, fn, A0, a1, A1, a2, A2)              \
  -(R)fn : (A0)a0 a1 : (A1)a1 a2 : (A2)a2 {                        \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0 a1:a1 a2:a2];                            \
  }
#define PROXY_METHOD4_SLOW(R, fn, A0, a1, A1, a2, A2, a3, A3)      \
  -(R)fn : (A0)a0 a1 : (A1)a1 a2 : (A2)a2 a3 : (A3)a3 {            \
    gl::ScopedProgressReporter scoped_reporter(progressReporter_); \
    return [device_ fn:a0 a1:a1 a2:a2 a3:a3];                      \
  }

// Disable availability warnings for the calls to |device_| in the macros. (The
// relevant availability guards are already present in the MTLDevice protocol
// for |self|).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

// Wrapped implementation of the MTLDevice protocol in which some methods
// have a gl::ScopedProgressReporter. The methods implemented using macros
// with the _SLOW suffix are the ones that create a gl::ScopedProgressReporter.
// The rule of thumb is that methods that could potentially do a GPU allocation
// or a shader compilation are marked as SLOW.
PROXY_METHOD0(NSString*, name)
PROXY_METHOD0(uint64_t, registryID)
PROXY_METHOD0(MTLSize, maxThreadsPerThreadgroup)
PROXY_METHOD0(BOOL, isLowPower)
PROXY_METHOD0(BOOL, isHeadless)
PROXY_METHOD0(BOOL, isRemovable)
PROXY_METHOD0(uint64_t, recommendedMaxWorkingSetSize)
PROXY_METHOD0(BOOL, isDepth24Stencil8PixelFormatSupported)
PROXY_METHOD0(MTLReadWriteTextureTier, readWriteTextureSupport)
PROXY_METHOD0(MTLArgumentBuffersTier, argumentBuffersSupport)
PROXY_METHOD0(BOOL, areRasterOrderGroupsSupported)
PROXY_METHOD0(NSUInteger, currentAllocatedSize)
PROXY_METHOD0(NSUInteger, maxThreadgroupMemoryLength)
PROXY_METHOD0(BOOL, areProgrammableSamplePositionsSupported)
PROXY_METHOD0_SLOW(nullable id<MTLCommandQueue>, newCommandQueue)
PROXY_METHOD1_SLOW(nullable id<MTLCommandQueue>,
                   newCommandQueueWithMaxCommandBufferCount,
                   NSUInteger)
PROXY_METHOD1(MTLSizeAndAlign,
              heapTextureSizeAndAlignWithDescriptor,
              MTLTextureDescriptor*)
PROXY_METHOD2(MTLSizeAndAlign,
              heapBufferSizeAndAlignWithLength,
              NSUInteger,
              options,
              MTLResourceOptions)
PROXY_METHOD1_SLOW(nullable id<MTLHeap>,
                   newHeapWithDescriptor,
                   MTLHeapDescriptor*)
PROXY_METHOD2_SLOW(nullable id<MTLBuffer>,
                   newBufferWithLength,
                   NSUInteger,
                   options,
                   MTLResourceOptions)
PROXY_METHOD3_SLOW(nullable id<MTLBuffer>,
                   newBufferWithBytes,
                   const void*,
                   length,
                   NSUInteger,
                   options,
                   MTLResourceOptions)
PROXY_METHOD4_SLOW(nullable id<MTLBuffer>,
                   newBufferWithBytesNoCopy,
                   void*,
                   length,
                   NSUInteger,
                   options,
                   MTLResourceOptions,
                   deallocator,
                   void (^__nullable)(void* pointer, NSUInteger length))
PROXY_METHOD1(nullable id<MTLDepthStencilState>,
              newDepthStencilStateWithDescriptor,
              MTLDepthStencilDescriptor*)
PROXY_METHOD1_SLOW(nullable id<MTLTexture>,
                   newTextureWithDescriptor,
                   MTLTextureDescriptor*)
PROXY_METHOD3_SLOW(nullable id<MTLTexture>,
                   newTextureWithDescriptor,
                   MTLTextureDescriptor*,
                   iosurface,
                   IOSurfaceRef,
                   plane,
                   NSUInteger)
PROXY_METHOD1_SLOW(nullable id<MTLSamplerState>,
                   newSamplerStateWithDescriptor,
                   MTLSamplerDescriptor*)
PROXY_METHOD0_SLOW(nullable id<MTLLibrary>, newDefaultLibrary)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newDefaultLibraryWithBundle,
                   NSBundle*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithFile,
                   NSString*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithURL,
                   NSURL*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithData,
                   dispatch_data_t,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD3_SLOW(nullable id<MTLLibrary>,
                   newLibraryWithSource,
                   NSString*,
                   options,
                   nullable MTLCompileOptions*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD3_SLOW(void,
                   newLibraryWithSource,
                   NSString*,
                   options,
                   nullable MTLCompileOptions*,
                   completionHandler,
                   MTLNewLibraryCompletionHandler)
PROXY_METHOD2_SLOW(nullable id<MTLRenderPipelineState>,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD4_SLOW(nullable id<MTLRenderPipelineState>,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   reflection,
                   MTLAutoreleasedRenderPipelineReflection* __nullable,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(void,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   completionHandler,
                   MTLNewRenderPipelineStateCompletionHandler)
PROXY_METHOD3_SLOW(void,
                   newRenderPipelineStateWithDescriptor,
                   MTLRenderPipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   completionHandler,
                   MTLNewRenderPipelineStateWithReflectionCompletionHandler)
PROXY_METHOD2_SLOW(nullable id<MTLComputePipelineState>,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD4_SLOW(nullable id<MTLComputePipelineState>,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   options,
                   MTLPipelineOption,
                   reflection,
                   MTLAutoreleasedComputePipelineReflection* __nullable,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD2_SLOW(void,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   completionHandler,
                   MTLNewComputePipelineStateCompletionHandler)
PROXY_METHOD3_SLOW(void,
                   newComputePipelineStateWithFunction,
                   id<MTLFunction>,
                   options,
                   MTLPipelineOption,
                   completionHandler,
                   MTLNewComputePipelineStateWithReflectionCompletionHandler)
PROXY_METHOD4_SLOW(nullable id<MTLComputePipelineState>,
                   newComputePipelineStateWithDescriptor,
                   MTLComputePipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   reflection,
                   MTLAutoreleasedComputePipelineReflection* __nullable,
                   error,
                   __autoreleasing NSError**)
PROXY_METHOD3_SLOW(void,
                   newComputePipelineStateWithDescriptor,
                   MTLComputePipelineDescriptor*,
                   options,
                   MTLPipelineOption,
                   completionHandler,
                   MTLNewComputePipelineStateWithReflectionCompletionHandler)
PROXY_METHOD0_SLOW(nullable id<MTLFence>, newFence)
PROXY_METHOD1(BOOL, supportsFeatureSet, MTLFeatureSet)
PROXY_METHOD1(BOOL, supportsTextureSampleCount, NSUInteger)
PROXY_METHOD1(NSUInteger,
              minimumLinearTextureAlignmentForPixelFormat,
              MTLPixelFormat)
PROXY_METHOD2(void,
              getDefaultSamplePositions,
              MTLSamplePosition*,
              count,
              NSUInteger)
PROXY_METHOD1_SLOW(nullable id<MTLArgumentEncoder>,
                   newArgumentEncoderWithArguments,
                   NSArray<MTLArgumentDescriptor*>*)
#if defined(MAC_OS_X_VERSION_10_14)
PROXY_METHOD1_SLOW(nullable id<MTLTexture>,
                   newSharedTextureWithDescriptor,
                   MTLTextureDescriptor*)
PROXY_METHOD1_SLOW(nullable id<MTLTexture>,
                   newSharedTextureWithHandle,
                   MTLSharedTextureHandle*)
PROXY_METHOD1(NSUInteger,
              minimumTextureBufferAlignmentForPixelFormat,
              MTLPixelFormat)
PROXY_METHOD0(NSUInteger, maxBufferLength)
PROXY_METHOD0(NSUInteger, maxArgumentBufferSamplerCount)
PROXY_METHOD3_SLOW(nullable id<MTLIndirectCommandBuffer>,
                   newIndirectCommandBufferWithDescriptor,
                   MTLIndirectCommandBufferDescriptor*,
                   maxCommandCount,
                   NSUInteger,
                   options,
                   MTLResourceOptions)
PROXY_METHOD0(nullable id<MTLEvent>, newEvent)
PROXY_METHOD0(nullable id<MTLSharedEvent>, newSharedEvent)
PROXY_METHOD1(nullable id<MTLSharedEvent>,
              newSharedEventWithHandle,
              MTLSharedEventHandle*)
#endif
#pragma clang diagnostic pop
@end
