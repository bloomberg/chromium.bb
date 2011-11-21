// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_PONG_PONG_H_
#define EXAMPLES_PONG_PONG_H_

#include <string>

#include "ppapi/c/pp_file_info.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"

namespace pp {
class FileIO;
class FileRef;
class FileSystem;
class Rect;
}  // namespace pp

namespace pong {

class View;

// The Instance class.  One of these exists for each instance of your NaCl
// module on the web page.  The browser will ask the Module object to create
// a new Instance for each occurrence of the <embed> tag that has these
// attributes:
//     type="application/x-nacl"
//     nacl="pong.nmf"
class Pong : public pp::Instance {
 public:
  explicit Pong(PP_Instance instance);
  virtual ~Pong();

  // Open the file (if available) that stores the game scores.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  // Update the graphics context to the new size, and regenerate |pixel_buffer_|
  // to fit the new size as well.
  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip);

  virtual bool HandleInputEvent(const pp::InputEvent& event);

  // Called by the browser to handle the postMessage() call in Javascript.
  // The message in this case is expected to contain the string 'update', or
  // 'resetScore' in order to invoke either the Update or ResetScore function
  // respectively.
  virtual void HandleMessage(const pp::Var& var_message);

  void Update();
  void UpdateScoreFromBuffer();
  void UpdateScoreFromFile();
  void WriteScoreToFile();

  PP_FileInfo file_info_;
  int32_t bytes_to_read_;
  int64_t offset_;
  pp::FileIO* file_io_;
  pp::FileRef* file_ref_;
  pp::FileSystem* file_system_;
  std::string bytes_buffer_;

 private:
  Pong(const Pong&);  // Disallow copy

  enum BallDirection {
    kUpDirection = 0,
    kDownDirection
  };
  void ResetPositions();
  void ResetScore();
  BallDirection RightPaddleNextMove() const;
  void UpdateScoreDisplay();

  View* view_;
  pp::Rect left_paddle_;
  pp::Rect right_paddle_;
  pp::Rect ball_;
  pp::Rect court_;
  int32_t delta_x_;
  int32_t delta_y_;
  int player_score_;
  int computer_score_;
};

}  // namespace pong

#endif  // EXAMPLES_PONG_PONG_H_
