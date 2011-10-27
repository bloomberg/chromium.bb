// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace gpu {
namespace gles2 {

ShaderManager::ShaderInfo::ShaderInfo(GLuint service_id, GLenum shader_type)
      : use_count_(0),
        service_id_(service_id),
        shader_type_(shader_type),
        valid_(false) {
}

ShaderManager::ShaderInfo::~ShaderInfo() {
}

void ShaderManager::ShaderInfo::IncUseCount() {
  ++use_count_;
}

void ShaderManager::ShaderInfo::DecUseCount() {
  --use_count_;
  DCHECK_GE(use_count_, 0);
}

void ShaderManager::ShaderInfo::MarkAsDeleted() {
  DCHECK_NE(service_id_, 0u);
  service_id_ = 0;
}

void ShaderManager::ShaderInfo::SetStatus(
    bool valid, const char* log, ShaderTranslatorInterface* translator) {
  valid_ = valid;
  log_info_.reset(log ? new std::string(log) : NULL);
  if (translator && valid) {
    attrib_map_ = translator->attrib_map();
    uniform_map_ = translator->uniform_map();
  } else {
    attrib_map_.clear();
    uniform_map_.clear();
  }
}

const ShaderManager::ShaderInfo::VariableInfo*
    ShaderManager::ShaderInfo::GetAttribInfo(
        const std::string& name) const {
  VariableMap::const_iterator it = attrib_map_.find(name);
  return it != attrib_map_.end() ? &it->second : NULL;
}

const ShaderManager::ShaderInfo::VariableInfo*
    ShaderManager::ShaderInfo::GetUniformInfo(
        const std::string& name) const {
  VariableMap::const_iterator it = uniform_map_.find(name);
  return it != uniform_map_.end() ? &it->second : NULL;
}

ShaderManager::ShaderManager() {}

ShaderManager::~ShaderManager() {
  DCHECK(shader_infos_.empty());
}

void ShaderManager::Destroy(bool have_context) {
  while (!shader_infos_.empty()) {
    if (have_context) {
      ShaderInfo* info = shader_infos_.begin()->second;
      if (!info->IsDeleted()) {
        glDeleteShader(info->service_id());
        info->MarkAsDeleted();
      }
    }
    shader_infos_.erase(shader_infos_.begin());
  }
}

ShaderManager::ShaderInfo* ShaderManager::CreateShaderInfo(
    GLuint client_id,
    GLuint service_id,
    GLenum shader_type) {
  std::pair<ShaderInfoMap::iterator, bool> result =
      shader_infos_.insert(std::make_pair(
          client_id, ShaderInfo::Ref(new ShaderInfo(service_id, shader_type))));
  DCHECK(result.second);
  return result.first->second;
}

ShaderManager::ShaderInfo* ShaderManager::GetShaderInfo(GLuint client_id) {
  ShaderInfoMap::iterator it = shader_infos_.find(client_id);
  return it != shader_infos_.end() ? it->second : NULL;
}

bool ShaderManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (ShaderInfoMap::const_iterator it = shader_infos_.begin();
       it != shader_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

bool ShaderManager::IsOwned(ShaderManager::ShaderInfo* info) {
  for (ShaderInfoMap::iterator it = shader_infos_.begin();
       it != shader_infos_.end(); ++it) {
    if (it->second.get() == info) {
      return true;
    }
  }
  return false;
}

void ShaderManager::RemoveShaderInfoIfUnused(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  if (info->IsDeleted() && !info->InUse()) {
    for (ShaderInfoMap::iterator it = shader_infos_.begin();
         it != shader_infos_.end(); ++it) {
      if (it->second.get() == info) {
        shader_infos_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }
}

void ShaderManager::MarkAsDeleted(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->MarkAsDeleted();
  RemoveShaderInfoIfUnused(info);
}

void ShaderManager::UseShader(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->IncUseCount();
}

void ShaderManager::UnuseShader(ShaderManager::ShaderInfo* info) {
  DCHECK(info);
  DCHECK(IsOwned(info));
  info->DecUseCount();
  RemoveShaderInfoIfUnused(info);
}

namespace {

// Strips comments from shader text. This allows non-ASCII characters
// to be used in comments without potentially breaking OpenGL
// implementations not expecting characters outside the GLSL ES set.
class Stripper {
 public:
  Stripper(const std::string& str)
    : parse_state_(kBeginningOfLine)
    , source_string_(str)
    , length_(str.length())
    , position_(0) {
    Parse();
  }

  const std::string& result() {
    return result_;
  }

 private:
  bool HasMoreCharacters() {
    return position_ < length_;
  }

  void Parse() {
    while (HasMoreCharacters()) {
      Process(Current());
      // Process() might Advance the position.
      if (HasMoreCharacters()) {
        Advance();
      }
    }
  }

  void Process(char);

  bool Peek(char* character) {
    DCHECK(character);
    if (position_ + 1 >= length_) {
      return false;
    }
    *character = source_string_[position_ + 1];
    return true;
  }

  char Current() {
    DCHECK_LT(position_, length_);
    return source_string_[position_];
  }

  void Advance() {
    ++position_;
  }

  bool IsNewline(char character) {
    // Don't attempt to canonicalize newline related characters.
    return (character == '\n' || character == '\r');
  }

  void Emit(char character) {
    result_.push_back(character);
  }

  enum ParseState {
    // Have not seen an ASCII non-whitespace character yet on
    // this line. Possible that we might see a preprocessor
    // directive.
    kBeginningOfLine,

    // Have seen at least one ASCII non-whitespace character
    // on this line.
    kMiddleOfLine,

    // Handling a preprocessor directive. Passes through all
    // characters up to the end of the line. Disables comment
    // processing.
    kInPreprocessorDirective,

    // Handling a single-line comment. The comment text is
    // replaced with a single space.
    kInSingleLineComment,

    // Handling a multi-line comment. Newlines are passed
    // through to preserve line numbers.
    kInMultiLineComment
  };

  ParseState parse_state_;
  std::string source_string_;
  unsigned length_;
  unsigned position_;
  std::string result_;
};

void Stripper::Process(char c) {
  if (IsNewline(c)) {
    // No matter what state we are in, pass through newlines
    // so we preserve line numbers.
    Emit(c);

    if (parse_state_ != kInMultiLineComment)
      parse_state_ = kBeginningOfLine;

    return;
  }

  char temp = 0;
  switch (parse_state_) {
    case kBeginningOfLine:
      if (IsAsciiWhitespace(c)) {
        Emit(c);
        break;
      }

      if (c == '#') {
        parse_state_ = kInPreprocessorDirective;
        Emit(c);
        break;
      }

      // Transition to normal state and re-handle character.
      parse_state_ = kMiddleOfLine;
      Process(c);
      break;

    case kMiddleOfLine:
      if (c == '/' && Peek(&temp)) {
        if (temp == '/') {
          parse_state_ = kInSingleLineComment;
          Emit(' ');
          Advance();
          break;
        }

        if (temp == '*') {
          parse_state_ = kInMultiLineComment;
          // Emit the comment start in case the user has
          // an unclosed comment and we want to later
          // signal an error.
          Emit('/');
          Emit('*');
          Advance();
          break;
        }
      }

      Emit(c);
      break;

    case kInPreprocessorDirective:
      // No matter what the character is, just pass it
      // through. Do not Parse comments in this state. This
      // might not be the right thing to do long term, but it
      // should handle the #error preprocessor directive.
      Emit(c);
      break;

    case kInSingleLineComment:
      // The newline code at the top of this function takes care
      // of resetting our state when we get out of the
      // single-line comment. Swallow all other characters.
      break;

    case kInMultiLineComment:
      if (c == '*' && Peek(&temp) && temp == '/') {
        Emit('*');
        Emit('/');
        parse_state_ = kMiddleOfLine;
        Advance();
        break;
      }

      // Swallow all other characters. Unclear whether we may
      // want or need to just Emit a space per character to try
      // to preserve column numbers for debugging purposes.
      break;
  }
}

}  // anonymous namespace

std::string ShaderManager::StripComments(const std::string source) {
  Stripper stripper(source);
  return stripper.result();
}

}  // namespace gles2
}  // namespace gpu


