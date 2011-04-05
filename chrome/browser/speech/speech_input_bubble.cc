// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_bubble.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

color_utils::HSL kGrayscaleShift = { -1, 0, 0.6 };

SkBitmap* mic_full_ = NULL;  // Mic image with full volume.
SkBitmap* mic_noise_ = NULL;  // Mic image with full noise volume.
SkBitmap* mic_empty_ = NULL;  // Mic image with zero volume.
SkBitmap* mic_mask_ = NULL;  // Gradient mask used by the volume indicator.
SkBitmap* spinner_ = NULL;  // Spinner image for the progress animation.

const int kWarmingUpAnimationStartMs = 500;
const int kWarmingUpAnimationStepMs = 100;
const int kRecognizingAnimationStepMs = 100;

}  // namespace

SpeechInputBubble::FactoryMethod SpeechInputBubble::factory_ = NULL;
const int SpeechInputBubble::kBubbleTargetOffsetX = 10;

SpeechInputBubble* SpeechInputBubble::Create(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect) {
  if (factory_)
    return (*factory_)(tab_contents, delegate, element_rect);

  // Has the tab already closed before bubble create request was processed?
  if (!tab_contents)
    return NULL;

  return CreateNativeBubble(tab_contents, delegate, element_rect);
}

SpeechInputBubbleBase::SpeechInputBubbleBase(TabContents* tab_contents)
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      display_mode_(DISPLAY_MODE_RECORDING),
      tab_contents_(tab_contents) {
  if (!mic_empty_) {  // Static variables.
    mic_empty_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_EMPTY);
    mic_noise_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_NOISE);
    mic_full_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_FULL);
    mic_mask_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_MASK);
    spinner_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_SPINNER);
  }

  // Instance variables.
  mic_image_.reset(new SkBitmap());
  mic_image_->setConfig(SkBitmap::kARGB_8888_Config, mic_empty_->width(),
                        mic_empty_->height());
  mic_image_->allocPixels();

  buffer_image_.reset(new SkBitmap());
  buffer_image_->setConfig(SkBitmap::kARGB_8888_Config, mic_empty_->width(),
                           mic_empty_->height());
  buffer_image_->allocPixels();

  // The sprite image consists of all the animation frames put together in one
  // horizontal/wide image. Each animation frame is square in shape within the
  // sprite.
  const int kFrameSize = spinner_->height();

  // When recording starts up, it may take a short while (few ms or even a
  // couple of seconds) before the audio device starts really capturing data.
  // This is more apparent on first use. To cover such cases we show a warming
  // up state in the bubble starting with a blank spinner image. If audio data
  // starts coming in within a couple hundred ms, we switch to the recording
  // UI and if it takes longer, we show the real warm up animation frames.
  // This reduces visual jank for the most part.
  // TODO(satish): Change this to create the frames only once on first use
  // instead of keeping them as instance variables in every bubble.
  SkBitmap empty_spinner;
  empty_spinner.setConfig(SkBitmap::kARGB_8888_Config, kFrameSize, kFrameSize);
  empty_spinner.allocPixels();
  empty_spinner.eraseRGB(255, 255, 255);
  warming_up_frames_.push_back(empty_spinner);

  for (SkIRect src_rect(SkIRect::MakeWH(kFrameSize, kFrameSize));
       src_rect.fLeft < spinner_->width();
       src_rect.offset(kFrameSize, 0)) {
    SkBitmap frame;
    spinner_->extractSubset(&frame, src_rect);

    // The bitmap created by extractSubset just points to the same pixels as
    // the original and adjusts rowBytes accordingly. However that doesn't
    // render properly and gets vertically squished in Linux due to a bug in
    // Skia. Until that gets fixed we work around by taking a real copy of it
    // below as the copied bitmap has the correct rowBytes and renders fine.
    SkBitmap frame_copy;
    frame.copyTo(&frame_copy, SkBitmap::kARGB_8888_Config);
    animation_frames_.push_back(frame_copy);

    // The warm up spinner animation is a gray scale version of the real one.
    warming_up_frames_.push_back(SkBitmapOperations::CreateHSLShiftedBitmap(
        frame_copy, kGrayscaleShift));
  }
}

SpeechInputBubbleBase::~SpeechInputBubbleBase() {
  // This destructor is added to make sure members such as the scoped_ptr
  // get destroyed here and the derived classes don't have to care about such
  // member variables which they don't use.
}

void SpeechInputBubbleBase::SetWarmUpMode() {
  task_factory_.RevokeAll();
  display_mode_ = DISPLAY_MODE_WARM_UP;
  animation_step_ = 0;
  DoWarmingUpAnimationStep();
  UpdateLayout();
}

void SpeechInputBubbleBase::DoWarmingUpAnimationStep() {
  SetImage(warming_up_frames_[animation_step_]);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(
          &SpeechInputBubbleBase::DoWarmingUpAnimationStep),
      animation_step_ == 0 ? kWarmingUpAnimationStartMs
                           : kWarmingUpAnimationStepMs);
  if (++animation_step_ >= static_cast<int>(animation_frames_.size()))
    animation_step_ = 1;  // Frame 0 is skipped during the animation.
}

void SpeechInputBubbleBase::SetRecordingMode() {
  task_factory_.RevokeAll();
  display_mode_ = DISPLAY_MODE_RECORDING;
  SetInputVolume(0, 0);
  UpdateLayout();
}

void SpeechInputBubbleBase::SetRecognizingMode() {
  display_mode_ = DISPLAY_MODE_RECOGNIZING;
  animation_step_ = 0;
  DoRecognizingAnimationStep();
  UpdateLayout();
}

void SpeechInputBubbleBase::DoRecognizingAnimationStep() {
  SetImage(animation_frames_[animation_step_]);
  if (++animation_step_ >= static_cast<int>(animation_frames_.size()))
    animation_step_ = 0;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(
          &SpeechInputBubbleBase::DoRecognizingAnimationStep),
      kRecognizingAnimationStepMs);
}

void SpeechInputBubbleBase::SetMessage(const string16& text) {
  task_factory_.RevokeAll();
  message_text_ = text;
  display_mode_ = DISPLAY_MODE_MESSAGE;
  UpdateLayout();
}

void SpeechInputBubbleBase::DrawVolumeOverlay(SkCanvas* canvas,
                                              const SkBitmap& bitmap,
                                              float volume) {
  buffer_image_->eraseARGB(0, 0, 0, 0);

  int width = mic_image_->width();
  int height = mic_image_->height();
  SkCanvas buffer_canvas(*buffer_image_);

  buffer_canvas.save();
  const int kVolumeSteps = 12;
  SkScalar clip_right =
      (((1.0f - volume) * (width * (kVolumeSteps + 1))) - width) / kVolumeSteps;
  buffer_canvas.clipRect(SkRect::MakeLTRB(0, 0,
      SkIntToScalar(width) - clip_right, SkIntToScalar(height)));
  buffer_canvas.drawBitmap(bitmap, 0, 0);
  buffer_canvas.restore();
  SkPaint multiply_paint;
  multiply_paint.setXfermode(SkXfermode::Create(SkXfermode::kMultiply_Mode));
  buffer_canvas.drawBitmap(*mic_mask_, -clip_right, 0, &multiply_paint);

  canvas->drawBitmap(*buffer_image_.get(), 0, 0);
}

void SpeechInputBubbleBase::SetInputVolume(float volume, float noise_volume) {
  mic_image_->eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(*mic_image_);

  // Draw the empty volume image first and the current volume image on top,
  // and then the noise volume image on top of both.
  canvas.drawBitmap(*mic_empty_, 0, 0);
  DrawVolumeOverlay(&canvas, *mic_full_, volume);
  DrawVolumeOverlay(&canvas, *mic_noise_, noise_volume);

  SetImage(*mic_image_.get());
}

TabContents* SpeechInputBubbleBase::tab_contents() {
  return tab_contents_;
}

void SpeechInputBubbleBase::SetImage(const SkBitmap& image) {
  icon_image_.reset(new SkBitmap(image));
  UpdateImage();
}

SkBitmap SpeechInputBubbleBase::icon_image() {
  return (icon_image_ != NULL) ? *icon_image_ : SkBitmap();
}
