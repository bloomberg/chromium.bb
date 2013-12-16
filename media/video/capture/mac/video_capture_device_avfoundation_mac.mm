// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/video_capture_device_avfoundation_mac.h"

#import <CoreVideo/CoreVideo.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#include "ui/gfx/size.h"

@implementation VideoCaptureDeviceAVFoundation

#pragma mark Class methods

+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames {
  // At this stage we already know that AVFoundation is supported and the whole
  // library is loaded and initialised, by the device monitoring.
  NSArray* devices = [AVCaptureDeviceGlue devices];
  for (CrAVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVFoundationGlue::AVMediaTypeVideo()] ||
        [device hasMediaType:AVFoundationGlue::AVMediaTypeMuxed()]) {
      [deviceNames setObject:[device localizedName]
                      forKey:[device uniqueID]];
    }
  }
}

+ (NSDictionary*)deviceNames {
  NSMutableDictionary* deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];
  // The device name retrieval is not going to happen in the main thread, and
  // this might cause instabilities (it did in QTKit), so keep an eye here.
  [self getDeviceNames:deviceNames];
  return deviceNames;
}

#pragma mark Public methods

- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  if ((self = [super init])) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(frameReceiver);
    [self setFrameReceiver:frameReceiver];
    captureSession_.reset(
        [[AVFoundationGlue::AVCaptureSessionClass() alloc] init]);
  }
  return self;
}

- (void)dealloc {
  [self stopCapture];
  [super dealloc];
}

- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  base::AutoLock lock(lock_);
  frameReceiver_ = frameReceiver;
}

- (BOOL)setCaptureDevice:(NSString*)deviceId {
  DCHECK(captureSession_);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!deviceId) {
    // First stop the capture session, if it's running.
    [self stopCapture];
    // Now remove the input and output from the capture session.
    [captureSession_ removeOutput:captureVideoDataOutput_];
    if (captureDeviceInput_) {
      [captureSession_ removeInput:captureDeviceInput_];
      // No need to release |captureDeviceInput_|, is owned by the session.
      captureDeviceInput_ = nil;
    }
    return YES;
  }

  // Look for input device with requested name.
  captureDevice_ = [AVCaptureDeviceGlue deviceWithUniqueID:deviceId];
  if (!captureDevice_) {
    DLOG(ERROR) << "Could not open video capture device.";
    return NO;
  }

  // Create the capture input associated with the device. Easy peasy.
  NSError* error = nil;
  captureDeviceInput_ = [AVCaptureDeviceInputGlue
      deviceInputWithDevice:captureDevice_
                      error:&error];
  if (!captureDeviceInput_) {
    captureDevice_ = nil;
    DLOG(ERROR) << "Could not create video capture input: "
                << [[error localizedDescription] UTF8String];
    return NO;
  }
  [captureSession_ addInput:captureDeviceInput_];

  // Create a new data output for video. The data output is configured to
  // discard late frames by default.
  captureVideoDataOutput_.reset(
      [[AVFoundationGlue::AVCaptureVideoDataOutputClass() alloc] init]);
  if (!captureVideoDataOutput_) {
    [captureSession_ removeInput:captureDeviceInput_];
    DLOG(ERROR) << "Could not create video data output.";
    return NO;
  }
  [captureVideoDataOutput_
      setSampleBufferDelegate:self
                        queue:dispatch_get_global_queue(
                            DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
  [captureSession_ addOutput:captureVideoDataOutput_];
  return YES;
}

- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate {
  DCHECK(thread_checker_.CalledOnValidThread());
  frameWidth_ = width;
  frameHeight_ = height;
  frameRate_ = frameRate;

  // Identify the sessionPreset that corresponds to the desired resolution.
  NSString* sessionPreset;
  if (width == 1280 && height == 720 && [captureSession_ canSetSessionPreset:
          AVFoundationGlue::AVCaptureSessionPreset1280x720()]) {
    sessionPreset = AVFoundationGlue::AVCaptureSessionPreset1280x720();
  } else if (width == 640 && height == 480 && [captureSession_
          canSetSessionPreset:
              AVFoundationGlue::AVCaptureSessionPreset640x480()]) {
    sessionPreset = AVFoundationGlue::AVCaptureSessionPreset640x480();
  } else if (width == 320 && height == 240 && [captureSession_
          canSetSessionPreset:
              AVFoundationGlue::AVCaptureSessionPreset320x240()]) {
    sessionPreset = AVFoundationGlue::AVCaptureSessionPreset320x240();
  } else {
    DLOG(ERROR) << "Unsupported resolution (" << width << "x" << height << ")";
    return NO;
  }
  [captureSession_ setSessionPreset:sessionPreset];

  // Check that our capture Device can be used with the current preset.
  if (![captureDevice_ supportsAVCaptureSessionPreset:
          [captureSession_ sessionPreset]]){
    DLOG(ERROR) << "Video capture device does not support current preset";
    return NO;
  }

  // Despite all Mac documentation detailing that setting the sessionPreset is
  // enough, that is not the case for, at least, the MacBook Air built-in
  // FaceTime HD Camera, and the capture output has to be configured as well.
  // The reason for this mismatch is probably because most of the AVFoundation
  // docs are written for iOS and not for MacOsX.
  // AVVideoScalingModeKey() refers to letterboxing yes/no and preserve aspect
  // ratio yes/no when scaling. Currently we set letterbox and preservation.
  NSDictionary* videoSettingsDictionary = @{
    (id)kCVPixelBufferWidthKey : @(width),
    (id)kCVPixelBufferHeightKey : @(height),
    (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_422YpCbCr8),
    AVFoundationGlue::AVVideoScalingModeKey() :
        AVFoundationGlue::AVVideoScalingModeResizeAspect()
  };
  [captureVideoDataOutput_ setVideoSettings:videoSettingsDictionary];

  CrAVCaptureConnection* captureConnection = [captureVideoDataOutput_
      connectionWithMediaType:AVFoundationGlue::AVMediaTypeVideo()];
  // TODO(mcasas): Check selector existence, related to bugs
  // http://crbug.com/327532 and http://crbug.com/328096.
  if ([captureConnection
           respondsToSelector:@selector(isVideoMinFrameDurationSupported)] &&
      [captureConnection isVideoMinFrameDurationSupported]) {
    [captureConnection setVideoMinFrameDuration:
        CoreMediaGlue::CMTimeMake(1, frameRate)];
  }
  if ([captureConnection
           respondsToSelector:@selector(isVideoMaxFrameDurationSupported)] &&
      [captureConnection isVideoMaxFrameDurationSupported]) {
    [captureConnection setVideoMaxFrameDuration:
        CoreMediaGlue::CMTimeMake(1, frameRate)];
  }
  return YES;
}

- (BOOL)startCapture {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!captureSession_) {
    DLOG(ERROR) << "Video capture session not initialized.";
    return NO;
  }
  // Connect the notifications.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self
         selector:@selector(onVideoError:)
             name:AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification()
           object:captureSession_];
  [captureSession_ startRunning];
  return YES;
}

- (void)stopCapture {
  DCHECK(thread_checker_.CalledOnValidThread());
  if ([captureSession_ isRunning])
    [captureSession_ stopRunning];  // Synchronous.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark Private methods

// |captureOutput| is called by the capture device to deliver a new frame.
- (void)captureOutput:(CrAVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CoreMediaGlue::CMSampleBufferRef)sampleBuffer
           fromConnection:(CrAVCaptureConnection*)connection {
  CVImageBufferRef videoFrame =
      CoreMediaGlue::CMSampleBufferGetImageBuffer(sampleBuffer);
  // Lock the frame and calculate frame size.
  const int kLockFlags = 0;
  if (CVPixelBufferLockBaseAddress(videoFrame, kLockFlags) ==
          kCVReturnSuccess) {
    void* baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
    size_t frameWidth = CVPixelBufferGetWidth(videoFrame);
    size_t frameHeight = CVPixelBufferGetHeight(videoFrame);
    size_t frameSize = bytesPerRow * frameHeight;
    UInt8* addressToPass = reinterpret_cast<UInt8*>(baseAddress);

    media::VideoCaptureFormat captureFormat(
        gfx::Size(frameWidth, frameHeight),
        frameRate_,
        media::PIXEL_FORMAT_UYVY);
    base::AutoLock lock(lock_);
    if (!frameReceiver_)
      return;
    frameReceiver_->ReceiveFrame(addressToPass, frameSize, captureFormat, 0, 0);
    CVPixelBufferUnlockBaseAddress(videoFrame, kLockFlags);
  }
}

- (void)onVideoError:(NSNotification*)errorNotification {
  NSError* error = base::mac::ObjCCast<NSError>([[errorNotification userInfo]
      objectForKey:AVFoundationGlue::AVCaptureSessionErrorKey()]);
  base::AutoLock lock(lock_);
  if (frameReceiver_)
    frameReceiver_->ReceiveError([[error localizedDescription] UTF8String]);
}

@end
