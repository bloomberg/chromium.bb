// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/pong/pong.h"

#include <stdio.h>
#include <cmath>
#include <string>
#include "examples/pong/view.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"

namespace {

const uint32_t kSpaceBar = 0x20;
const uint32_t kUpArrow = 0x26;
const uint32_t kDownArrow = 0x28;

const int32_t kMaxPointsAllowed = 256;
const std::string kResetScoreMethodId = "resetScore";
const uint32_t kUpdateDistance = 4;
const int32_t kUpdateInterval = 17;  // milliseconds

}  // namespace

namespace pong {

// Callbacks that are called asynchronously by the system as a result of various
// pp::FileIO methods.
namespace AsyncCallbacks {
// Callback that is called as a result of pp::FileIO::Flush
void FlushCallback(void*, int32_t) {
}

// Callback that is called as a result of pp::FileIO::Write
void WriteCallback(void* data, int32_t bytes_written) {
  if (bytes_written < 0)
    return;  // error
  Pong* pong = static_cast<Pong*>(data);
  pong->offset_ += bytes_written;
  if (pong->offset_ == pong->bytes_buffer_.length()) {
    pong->file_io_->Flush(pp::CompletionCallback(FlushCallback, NULL));
  } else {
    // Not all the bytes to be written have been written, so call
    // pp::FileIO::Write again.
    pong->file_io_->Write(pong->offset_, &pong->bytes_buffer_[pong->offset_],
                          pong->bytes_buffer_.length() - pong->offset_,
                          pp::CompletionCallback(WriteCallback, pong));
  }
}

// Callback that is called as a result of pp::FileSystem::Open
void FileSystemOpenCallback(void* data, int32_t result) {
  if (result != PP_OK)
    return;
  Pong* pong = static_cast<Pong*>(data);
  pong->UpdateScoreFromFile();
}

// Callback that is called as a result of pp::FileIO::Read
void ReadCallback(void* data, int32_t bytes_read) {
  if (bytes_read < 0)
    return;  // error
  Pong* pong = static_cast<Pong*>(data);
  pong->bytes_to_read_ -= bytes_read;
  if (pong->bytes_to_read_ ==  0) {
    // File has been read to completion.  Parse the bytes to get the scores.
    pong->UpdateScoreFromBuffer();
  } else {
    pong->offset_ += bytes_read;
    pong->file_io_->Read(pong->offset_,
                         &pong->bytes_buffer_[pong->offset_],
                         pong->bytes_to_read_,
                         pp::CompletionCallback(ReadCallback, pong));
  }
}

// Callback that is called as a result of pp::FileIO::Query
void QueryCallback(void* data, int32_t result) {
  if (result != PP_OK)
    return;
  Pong* pong = static_cast<Pong*>(data);
  pong->bytes_to_read_ = pong->file_info_.size;
  pong->offset_ = 0;
  pong->bytes_buffer_.resize(pong->bytes_to_read_);
  pong->file_io_->Read(pong->offset_,
                       &pong->bytes_buffer_[0],
                       pong->bytes_to_read_,
                       pp::CompletionCallback(ReadCallback, pong));
}

// Callback that is called as a result of pp::FileIO::Open
void FileOpenCallback(void*data, int32_t result) {
  if (result != PP_OK) {
    return;
  }
  Pong* pong = static_cast<Pong*>(data);
  // Query the file in order to get the file size.
  pong->file_io_->Query(&pong->file_info_, pp::CompletionCallback(QueryCallback,
                                                                  pong));
}

// Callback that is called as a result of pp::Core::CallOnMainThread
void UpdateCallback(void* data, int32_t /*result*/) {
  Pong* pong = static_cast<Pong*>(data);
  pong->Update();
}

}  // namespace AsyncCallbacks

class UpdateScheduler {
 public:
  UpdateScheduler(int32_t delay, Pong* pong)
      : delay_(delay), pong_(pong) {}
  ~UpdateScheduler() {
    pp::Core* core = pp::Module::Get()->core();
    core->CallOnMainThread(delay_, pp::CompletionCallback(
        AsyncCallbacks::UpdateCallback, pong_));
  }

 private:
  int32_t delay_;  // milliseconds
  Pong* pong_;  // weak
};

Pong::Pong(PP_Instance instance)
    : pp::Instance(instance),
      bytes_to_read_(0),
      offset_(0),
      file_io_(NULL),
      file_ref_(NULL),
      file_system_(NULL),
      view_(NULL),
      delta_x_(0),
      delta_y_(0),
      player_score_(0),
      computer_score_(0) {
  // Request to receive input events.
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_KEYBOARD);
}

Pong::~Pong() {
  delete view_;
  file_io_->Close();
  delete file_io_;
  delete file_ref_;
  delete file_system_;
}

bool Pong::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  view_ = new View(this);
  // Read the score from file.
  file_system_ = new pp::FileSystem(this, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  file_ref_ = new pp::FileRef(*file_system_, "/pong_score");
  // We kick off a series of callbacks which open a file, query the file for
  // it's length in bytes, read the file contents, and then update the score
  // display based on the file contents.
  int32_t rv = file_system_->Open(
      1024, pp::CompletionCallback(AsyncCallbacks::FileSystemOpenCallback,
                                   this));
  if (rv != PP_OK_COMPLETIONPENDING) {
    PostMessage(pp::Var("ERROR: Could not open local persistent file system."));
    return true;
  }
  UpdateScoreDisplay();
  UpdateScheduler(kUpdateInterval, this);
  return true;
}

void Pong::DidChangeView(const pp::Rect& position,
                         const pp::Rect& clip) {
  pp::Size view_size = view_->GetSize();
  const bool view_was_empty = view_size.IsEmpty();
  view_->UpdateView(position, clip, this);
  if (view_was_empty)
    ResetPositions();
}

bool Pong::HandleInputEvent(const pp::InputEvent& event) {
  if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEUP) {
    // By notifying the browser mouse clicks are handled, the application window
    // is able to get focus and receive key events.
    return true;
  } else if (event.GetType() == PP_INPUTEVENT_TYPE_KEYUP) {
    pp::KeyboardInputEvent key = pp::KeyboardInputEvent(event);
    return view_->KeyUp(key);
  } else if (event.GetType() == PP_INPUTEVENT_TYPE_KEYDOWN) {
    pp::KeyboardInputEvent key = pp::KeyboardInputEvent(event);
    return view_->KeyDown(key);
  }
  return false;
}


void Pong::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string())
    return;
  std::string message = var_message.AsString();
  if (message == kResetScoreMethodId) {
    ResetScore();
  }
}

void Pong::Update() {
  // Schedule another update
  UpdateScheduler(kUpdateInterval, this);

  const uint32_t key_code = view_->last_key_code();
  if (key_code == kSpaceBar) {
    ResetPositions();
    return;
  }
  if (ball_.right() < court_.x()) {
    ++computer_score_;
    if (computer_score_ > kMaxPointsAllowed) {
      ResetScore();
    } else {
      WriteScoreToFile();
      UpdateScoreDisplay();
    }
    ResetPositions();
    return;
  }
  if (ball_.x() > court_.right()) {
    ++player_score_;
    if (player_score_ > kMaxPointsAllowed) {
      ResetScore();
    } else {
      WriteScoreToFile();
      UpdateScoreDisplay();
    }
    ResetPositions();
    return;
  }
  // Update human controlled paddle
  if (key_code == kUpArrow) {
    left_paddle_.Offset(0, -kUpdateDistance);
    if (left_paddle_.y() - 1 < court_.y()) {
      left_paddle_.Offset(0, court_.y() - left_paddle_.y() + 1);
    }
  } else if (key_code == kDownArrow) {
    left_paddle_.Offset(0, kUpdateDistance);
    if (left_paddle_.bottom() + 1 > court_.bottom()) {
      left_paddle_.Offset(0, court_.bottom() - left_paddle_.bottom() - 1);
    }
  }

  // Update AI controlled paddle
  BallDirection direction = RightPaddleNextMove();
  if (direction == kUpDirection) {
    right_paddle_.Offset(0, -kUpdateDistance);
    if (right_paddle_.y() < court_.y() + 1) {
      right_paddle_.Offset(0, court_.y() - right_paddle_.y() + 1);
    }
  } else if (direction == kDownDirection) {
    right_paddle_.Offset(0, kUpdateDistance);
    if (right_paddle_.bottom() > court_.bottom() - 1) {
      right_paddle_.Offset(0, court_.bottom() - right_paddle_.bottom() - 1);
    }
  }

  // Bounce ball off bottom of screen
  if (ball_.bottom() >= court_.bottom() - 1) {
    ball_.Offset(0, court_.bottom() - ball_.bottom() - 1);
    delta_y_ = -delta_y_;
  }
  // Bounce ball off top of screen
  if (ball_.y() <= court_.y() + 1) {
    ball_.Offset(0, court_.y() - ball_.y() + 1);
    delta_y_ = -delta_y_;
  }
  // Bounce ball off human controlled paddle
  if (left_paddle_.Intersects(ball_)) {
    delta_x_ = abs(delta_x_);
    if (ball_.CenterPoint().y() <
        left_paddle_.y() + left_paddle_.height() / 5) {
      delta_y_ -= kUpdateDistance;
      if (delta_y_ == 0)
        delta_y_ = -kUpdateDistance;
    } else if (ball_.CenterPoint().y() >
               left_paddle_.bottom() - left_paddle_.height() / 5) {
      delta_y_ += kUpdateDistance;
      if (delta_y_ == 0)
        delta_y_ = kUpdateDistance;
    }
  }
  // Bounce ball off ai controlled paddle
  if (right_paddle_.Intersects(ball_)) {
    delta_x_ = -abs(delta_x_);
    if (ball_.CenterPoint().y() >
        right_paddle_.bottom() - right_paddle_.height() / 5) {
      delta_y_ += kUpdateDistance;
      if (delta_y_ == 0)
        delta_y_ = kUpdateDistance;
    } else if (ball_.CenterPoint().y() <
               right_paddle_.y() + right_paddle_.height() / 5) {
      delta_y_ -= kUpdateDistance;
      if (delta_y_ == 0)
        delta_y_ -= kUpdateDistance;
    }
  }

  // Move ball
  ball_.Offset(delta_x_, delta_y_);

  view_->set_ball_rect(ball_);
  view_->set_left_paddle_rect(left_paddle_);
  view_->set_right_paddle_rect(right_paddle_);
  view_->Draw();
}

void Pong::UpdateScoreFromBuffer() {
  size_t pos = bytes_buffer_.find_first_of(':');
  player_score_ = ::atoi(bytes_buffer_.substr(0, pos).c_str());
  computer_score_ = ::atoi(bytes_buffer_.substr(pos + 1).c_str());
  UpdateScoreDisplay();
}

void Pong::UpdateScoreFromFile() {
  file_io_ = new pp::FileIO(this);
  file_io_->Open(*file_ref_,
                 PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE |
                 PP_FILEOPENFLAG_CREATE,
                 pp::CompletionCallback(AsyncCallbacks::FileOpenCallback,
                                        this));
}

void Pong::WriteScoreToFile() {
  if (file_io_ == NULL)
    return;
  // Write the score in <player score>:<computer score> format.
  size_t score_string_length = 1 + (player_score_ ? log(player_score_) : 1) + 1
      + (computer_score_ ? log(computer_score_) : 1) + 1;
  bytes_buffer_.resize(score_string_length);
  snprintf(&bytes_buffer_[0], bytes_buffer_.length(), "%i:%i", player_score_,
           computer_score_);
  offset_ = 0;  // overwrite score in file.
  file_io_->Write(offset_,
                  bytes_buffer_.c_str(),
                  bytes_buffer_.length(),
                  pp::CompletionCallback(AsyncCallbacks::WriteCallback, this));
}

void Pong::ResetPositions() {
  pp::Size court_size = view_->GetSize();
  pp::Rect court_rect(court_size);
  court_.SetRect(court_rect);

  pp::Rect rect;
  rect.set_width(20);
  rect.set_height(20);
  rect.set_x((court_rect.x() + court_rect.width()) / 2 - rect.width() / 2);
  rect.set_y(court_rect.y() + court_rect.height() - rect.height());
  ball_.SetRect(rect);

  const float paddle_width = 10;
  const float paddle_height = 99;
  const float paddle_pos_y =
      (court_rect.y() + court_rect.height()) / 2 - rect.height() / 2;
  rect.set_width(paddle_width);
  rect.set_height(paddle_height);
  rect.set_x(court_rect.x() + court_rect.width() / 5 + 1);
  rect.set_y(paddle_pos_y);
  left_paddle_.SetRect(rect);

  rect.set_width(paddle_width);
  rect.set_height(paddle_height);
  rect.set_x(court_rect.x() + 4 * court_rect.width() / 5 - rect.width() - 1);
  rect.set_y(paddle_pos_y);
  right_paddle_.SetRect(rect);

  delta_x_ = delta_y_ = kUpdateDistance;
}

void Pong::ResetScore() {
  player_score_ = 0;
  computer_score_ = 0;
  UpdateScoreDisplay();
}

Pong::BallDirection Pong::RightPaddleNextMove() const {
  static int32_t last_ball_y = 0;
  int32_t ball_y = ball_.CenterPoint().y();
  BallDirection ball_direction =
      ball_y < last_ball_y ? kUpDirection : kDownDirection;
  last_ball_y = ball_y;

  if (ball_y < right_paddle_.y())
    return kUpDirection;
  if (ball_y > right_paddle_.bottom())
    return kDownDirection;
  return ball_direction;
}

void Pong::UpdateScoreDisplay() {
  if (file_io_ == NULL)
    return;  //  Since we cant't save the score to file, do nothing and return.
  size_t message_length = 18 + (player_score_ ? log(player_score_) : 1) + 1
      + (computer_score_ ? log(computer_score_) : 1) + 1;
  std::string score_message(message_length, '\0');
  snprintf(&score_message[0], score_message.length(),
           "You: %i   Computer: %i", player_score_, computer_score_);
  PostMessage(pp::Var(score_message));
}

}  // namespace pong
