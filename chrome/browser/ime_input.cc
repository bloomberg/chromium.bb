// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ime_input.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkColor.h"

// "imm32.lib" is required by IMM32 APIs used in this file.
// NOTE(hbono): To comply with a comment from Darin, I have added
// this #pragma directive instead of adding "imm32.lib" to a project file.
#pragma comment(lib, "imm32.lib")

///////////////////////////////////////////////////////////////////////////////
// ImeInput

namespace {

// Determines whether or not the given attribute represents a target
// (a.k.a. a selection).
bool IsTargetAttribute(char attribute) {
  return (attribute == ATTR_TARGET_CONVERTED ||
          attribute == ATTR_TARGET_NOTCONVERTED);
}

// Helper function for ImeInput::GetCompositionInfo() method, to get the target
// range that's selected by the user in the current composition string.
void GetCompositionTargetRange(HIMC imm_context, int* target_start,
                               int* target_end) {
  int attribute_size = ::ImmGetCompositionString(imm_context, GCS_COMPATTR,
                                                 NULL, 0);
  if (attribute_size > 0) {
    int start = 0;
    int end = 0;
    scoped_array<char> attribute_data(new char[attribute_size]);
    if (attribute_data.get()) {
      ::ImmGetCompositionString(imm_context, GCS_COMPATTR,
                                attribute_data.get(), attribute_size);
      for (start = 0; start < attribute_size; ++start) {
        if (IsTargetAttribute(attribute_data[start]))
          break;
      }
      for (end = start; end < attribute_size; ++end) {
        if (!IsTargetAttribute(attribute_data[end]))
          break;
      }
      if (start == attribute_size) {
        // This composition clause does not contain any target clauses,
        // i.e. this clauses is an input clause.
        // We treat the whole composition as a target clause.
        start = 0;
        end = attribute_size;
      }
    }
    *target_start = start;
    *target_end = end;
  }
}

// Helper function for ImeInput::GetCompositionInfo() method, to get underlines
// information of the current composition string.
void GetCompositionUnderlines(
    HIMC imm_context,
    int target_start,
    int target_end,
    std::vector<WebKit::WebCompositionUnderline>* underlines) {
  int clause_size = ::ImmGetCompositionString(imm_context, GCS_COMPCLAUSE,
                                              NULL, 0);
  int clause_length = clause_size / sizeof(uint32);
  if (clause_length) {
    scoped_array<uint32> clause_data(new uint32[clause_length]);
    if (clause_data.get()) {
      ::ImmGetCompositionString(imm_context, GCS_COMPCLAUSE,
                                clause_data.get(), clause_size);
      for (int i = 0; i < clause_length - 1; ++i) {
        WebKit::WebCompositionUnderline underline;
        underline.startOffset = clause_data[i];
        underline.endOffset = clause_data[i+1];
        underline.color = SK_ColorBLACK;
        underline.thick = false;

        // Use thick underline for the target clause.
        if (underline.startOffset >= static_cast<unsigned>(target_start) &&
            underline.endOffset <= static_cast<unsigned>(target_end)) {
          underline.thick = true;
        }
        underlines->push_back(underline);
      }
    }
  }
}

}  // namespace

ImeInput::ImeInput()
    : ime_status_(false),
      input_language_id_(LANG_USER_DEFAULT),
      is_composing_(false),
      system_caret_(false),
      caret_rect_(-1, -1, 0, 0) {
}

ImeInput::~ImeInput() {
}

bool ImeInput::SetInputLanguage() {
  // Retrieve the current keyboard layout from Windows and determine whether
  // or not the current input context has IMEs.
  // Also save its input language for language-specific operations required
  // while composing a text.
  HKL keyboard_layout = ::GetKeyboardLayout(0);
  input_language_id_ = reinterpret_cast<LANGID>(keyboard_layout);
  ime_status_ = (::ImmIsIME(keyboard_layout) == TRUE) ? true : false;
  return ime_status_;
}


void ImeInput::CreateImeWindow(HWND window_handle) {
  // When a user disables TSF (Text Service Framework) and CUAS (Cicero
  // Unaware Application Support), Chinese IMEs somehow ignore function calls
  // to ::ImmSetCandidateWindow(), i.e. they do not move their candidate
  // window to the position given as its parameters, and use the position
  // of the current system caret instead, i.e. it uses ::GetCaretPos() to
  // retrieve the position of their IME candidate window.
  // Therefore, we create a temporary system caret for Chinese IMEs and use
  // it during this input context.
  // Since some third-party Japanese IME also uses ::GetCaretPos() to determine
  // their window position, we also create a caret for Japanese IMEs.
  if (PRIMARYLANGID(input_language_id_) == LANG_CHINESE ||
      PRIMARYLANGID(input_language_id_) == LANG_JAPANESE) {
    if (!system_caret_) {
      if (::CreateCaret(window_handle, NULL, 1, 1)) {
        system_caret_ = true;
      }
    }
  }
  // Restore the positions of the IME windows.
  UpdateImeWindow(window_handle);
}

void ImeInput::SetImeWindowStyle(HWND window_handle, UINT message,
                                 WPARAM wparam, LPARAM lparam,
                                 BOOL* handled) {
  // To prevent the IMM (Input Method Manager) from displaying the IME
  // composition window, Update the styles of the IME windows and EXPLICITLY
  // call ::DefWindowProc() here.
  // NOTE(hbono): We can NEVER let WTL call ::DefWindowProc() when we update
  // the styles of IME windows because the 'lparam' variable is a local one
  // and all its updates disappear in returning from this function, i.e. WTL
  // does not call ::DefWindowProc() with our updated 'lparam' value but call
  // the function with its original value and over-writes our window styles.
  *handled = TRUE;
  lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
  ::DefWindowProc(window_handle, message, wparam, lparam);
}

void ImeInput::DestroyImeWindow(HWND window_handle) {
  // Destroy the system caret if we have created for this IME input context.
  if (system_caret_) {
    ::DestroyCaret();
    system_caret_ = false;
  }
}

void ImeInput::MoveImeWindow(HWND window_handle, HIMC imm_context) {
  int x = caret_rect_.x();
  int y = caret_rect_.y();
  const int kCaretMargin = 1;
  // As written in a comment in ImeInput::CreateImeWindow(),
  // Chinese IMEs ignore function calls to ::ImmSetCandidateWindow()
  // when a user disables TSF (Text Service Framework) and CUAS (Cicero
  // Unaware Application Support).
  // On the other hand, when a user enables TSF and CUAS, Chinese IMEs
  // ignore the position of the current system caret and uses the
  // parameters given to ::ImmSetCandidateWindow() with its 'dwStyle'
  // parameter CFS_CANDIDATEPOS.
  // Therefore, we do not only call ::ImmSetCandidateWindow() but also
  // set the positions of the temporary system caret if it exists.
  CANDIDATEFORM candidate_position = {0, CFS_CANDIDATEPOS, {x, y},
                                      {0, 0, 0, 0}};
  ::ImmSetCandidateWindow(imm_context, &candidate_position);
  if (system_caret_) {
    switch (PRIMARYLANGID(input_language_id_)) {
      case LANG_JAPANESE:
        ::SetCaretPos(x, y + caret_rect_.height());
        break;
      default:
        ::SetCaretPos(x, y);
        break;
    }
  }
  if (PRIMARYLANGID(input_language_id_) == LANG_KOREAN) {
    // Chinese IMEs and Japanese IMEs require the upper-left corner of
    // the caret to move the position of their candidate windows.
    // On the other hand, Korean IMEs require the lower-left corner of the
    // caret to move their candidate windows.
    y += kCaretMargin;
  }
  // Japanese IMEs and Korean IMEs also use the rectangle given to
  // ::ImmSetCandidateWindow() with its 'dwStyle' parameter CFS_EXCLUDE
  // to move their candidate windows when a user disables TSF and CUAS.
  // Therefore, we also set this parameter here.
  CANDIDATEFORM exclude_rectangle = {0, CFS_EXCLUDE, {x, y},
      {x, y, x + caret_rect_.width(), y + caret_rect_.height()}};
  ::ImmSetCandidateWindow(imm_context, &exclude_rectangle);
}

void ImeInput::UpdateImeWindow(HWND window_handle) {
  // Just move the IME window attached to the given window.
  if (caret_rect_.x() >= 0 && caret_rect_.y() >= 0) {
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      MoveImeWindow(window_handle, imm_context);
      ::ImmReleaseContext(window_handle, imm_context);
    }
  }
}

void ImeInput::CleanupComposition(HWND window_handle) {
  // Notify the IMM attached to the given window to complete the ongoing
  // composition, (this case happens when the given window is de-activated
  // while composing a text and re-activated), and reset the omposition status.
  if (is_composing_) {
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      ::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
      ::ImmReleaseContext(window_handle, imm_context);
    }
    ResetComposition(window_handle);
  }
}

void ImeInput::ResetComposition(HWND window_handle) {
  // Currently, just reset the composition status.
  is_composing_ = false;
}

void ImeInput::CompleteComposition(HWND window_handle, HIMC imm_context) {
  // We have to confirm there is an ongoing composition before completing it.
  // This is for preventing some IMEs from getting confused while completing an
  // ongoing composition even if they do not have any ongoing compositions.)
  if (is_composing_) {
    ::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ResetComposition(window_handle);
  }
}

void ImeInput::GetCompositionInfo(HIMC imm_context, LPARAM lparam,
                                  ImeComposition* composition) {
  // We only care about GCS_COMPATTR, GCS_COMPCLAUSE and GCS_CURSORPOS, and
  // convert them into underlines and selection range respectively.
  composition->underlines.clear();

  int length = static_cast<int>(composition->ime_string.length());

  // Retrieve the selection range information. If CS_NOMOVECARET is specified,
  // that means the cursor should not be moved, then we just place the caret at
  // the beginning of the composition string. Otherwise we should honour the
  // GCS_CURSORPOS value if it's available.
  // TODO(suzhe): due to a bug of webkit, we currently can't use selection range
  // with composition string. See: https://bugs.webkit.org/show_bug.cgi?id=40805
  if (lparam & CS_NOMOVECARET) {
    composition->selection_start = composition->selection_end = 0;
  } else if (lparam & GCS_CURSORPOS) {
    composition->selection_start = composition->selection_end =
        ::ImmGetCompositionString(imm_context, GCS_CURSORPOS, NULL, 0);
  } else {
    composition->selection_start = composition->selection_end = length;
  }

  // Find out the range selected by the user.
  int target_start = 0;
  int target_end = 0;
  if (lparam & GCS_COMPATTR)
    GetCompositionTargetRange(imm_context, &target_start, &target_end);

  // Retrieve the clause segmentations and convert them to underlines.
  if (lparam & GCS_COMPCLAUSE) {
    GetCompositionUnderlines(imm_context, target_start, target_end,
                             &composition->underlines);
  }

  // Set default underlines in case there is no clause information.
  if (!composition->underlines.size()) {
    WebKit::WebCompositionUnderline underline;
    underline.color = SK_ColorBLACK;
    if (target_start > 0) {
      underline.startOffset = 0;
      underline.endOffset = target_start;
      underline.thick = false;
      composition->underlines.push_back(underline);
    }
    if (target_end > target_start) {
      underline.startOffset = target_start;
      underline.endOffset = target_end;
      underline.thick = true;
      composition->underlines.push_back(underline);
    }
    if (target_end < length) {
      underline.startOffset = target_end;
      underline.endOffset = length;
      underline.thick = false;
      composition->underlines.push_back(underline);
    }
  }
}

bool ImeInput::GetString(HIMC imm_context, WPARAM lparam, int type,
                         ImeComposition* composition) {
  bool result = false;
  if (lparam & type) {
    int string_size = ::ImmGetCompositionString(imm_context, type, NULL, 0);
    if (string_size > 0) {
      int string_length = string_size / sizeof(wchar_t);
      wchar_t *string_data = WriteInto(&composition->ime_string,
                                       string_length + 1);
      if (string_data) {
        // Fill the given ImeComposition object.
        ::ImmGetCompositionString(imm_context, type,
                                  string_data, string_size);
        composition->string_type = type;
        result = true;
      }
    }
  }
  return result;
}

bool ImeInput::GetResult(HWND window_handle, LPARAM lparam,
                         ImeComposition* composition) {
  bool result = false;
  HIMC imm_context = ::ImmGetContext(window_handle);
  if (imm_context) {
    // Copy the result string to the ImeComposition object.
    result = GetString(imm_context, lparam, GCS_RESULTSTR, composition);
    // Reset all the other parameters because a result string does not
    // have composition attributes.
    composition->selection_start = 0;
    composition->selection_end = 0;
    ::ImmReleaseContext(window_handle, imm_context);
  }
  return result;
}

bool ImeInput::GetComposition(HWND window_handle, LPARAM lparam,
                              ImeComposition* composition) {
  bool result = false;
  HIMC imm_context = ::ImmGetContext(window_handle);
  if (imm_context) {
    // Copy the composition string to the ImeComposition object.
    result = GetString(imm_context, lparam, GCS_COMPSTR, composition);

    // This is a dirty workaround for facebook. Facebook deletes the placeholder
    // character (U+3000) used by Traditional-Chinese IMEs at the beginning of
    // composition text. This prevents WebKit from replacing this placeholder
    // character with a Traditional-Chinese character, i.e. we cannot input any
    // characters in a comment box of facebook with Traditional-Chinese IMEs.
    // As a workaround, we replace U+3000 at the beginning of composition text
    // with U+FF3F, a placeholder character used by Japanese IMEs.
    if (input_language_id_ == MAKELANGID(LANG_CHINESE,
                                         SUBLANG_CHINESE_TRADITIONAL) &&
        composition->ime_string[0] == 0x3000) {
      composition->ime_string[0] = 0xFF3F;
    }

    // Retrieve the composition underlines and selection range information.
    GetCompositionInfo(imm_context, lparam, composition);

    // Mark that there is an ongoing composition.
    is_composing_ = true;

    ::ImmReleaseContext(window_handle, imm_context);
  }
  return result;
}

void ImeInput::DisableIME(HWND window_handle) {
  // A renderer process have moved its input focus to a password input
  // when there is an ongoing composition, e.g. a user has clicked a
  // mouse button and selected a password input while composing a text.
  // For this case, we have to complete the ongoing composition and
  // clean up the resources attached to this object BEFORE DISABLING THE IME.
  CleanupComposition(window_handle);
  ::ImmAssociateContextEx(window_handle, NULL, 0);
}

void ImeInput::CancelIME(HWND window_handle) {
  if (is_composing_) {
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      ::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
      ::ImmReleaseContext(window_handle, imm_context);
    }
    ResetComposition(window_handle);
  }
}

void ImeInput::EnableIME(HWND window_handle) {
  // Load the default IME context.
  // NOTE(hbono)
  //   IMM ignores this call if the IME context is loaded. Therefore, we do
  //   not have to check whether or not the IME context is loaded.
  ::ImmAssociateContextEx(window_handle, NULL, IACE_DEFAULT);
}

void ImeInput::UpdateCaretRect(HWND window_handle,
                               const gfx::Rect& caret_rect) {
  // Save the caret position, and Update the position of the IME window.
  // This update is used for moving an IME window when a renderer process
  // resize/moves the input caret.
  if (caret_rect_ != caret_rect) {
    caret_rect_ = caret_rect;
    // Move the IME windows.
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      MoveImeWindow(window_handle, imm_context);
      ::ImmReleaseContext(window_handle, imm_context);
    }
  }
}
