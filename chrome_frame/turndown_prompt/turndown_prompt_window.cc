// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/turndown_prompt/turndown_prompt_window.h"

#include <atlctrls.h>
#include <commctrl.h>
#include <shellapi.h>

#include "base/compiler_specific.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/utils.h"
#include "grit/chrome_frame_dialogs.h"
#include "grit/chrome_frame_resources.h"
#include "grit/chromium_strings.h"

// atlctrlx.h requires 'min' and 'max' macros, the definition of which conflicts
// with STL headers. Hence we include them out of the order defined by style
// guidelines. As a result you may not refer to std::min or std::max in this
// file.
#include <minmax.h>  // NOLINT
#include <atlctrlx.h>  // NOLINT

namespace {
const uint32 kBitmapImageSize = 18;
}  // namespace

// WTL's CBitmapButton's drawing code is horribly broken when using transparent
// images (specifically, it doesn't clear the background between redraws).
// Fix it here.
class CFBitmapButton: public CBitmapButtonImpl<CFBitmapButton>
{
 public:
  DECLARE_WND_SUPERCLASS(_T("WTL_BitmapButton"), GetWndClassName())

  CFBitmapButton()
      : CBitmapButtonImpl<CFBitmapButton>(BMPBTN_AUTOSIZE | BMPBTN_HOVER,
                                          NULL) {}

  // "Overridden" from CBitmapButtonImpl via template hackery. See
  // CBitmapButtonImpl::OnPaint() in atlctrlx.h for details.
  void DoPaint(CDCHandle dc) {
    RECT rc = {0};
    GetClientRect(&rc);
    dc.FillRect(&rc, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));

    // Call original implementation.
    CBitmapButtonImpl<CFBitmapButton>::DoPaint(dc);
  }
};

// static
base::WeakPtr<TurndownPromptWindow> TurndownPromptWindow::CreateInstance(
    InfobarContent::Frame* frame,
    UrlLauncher* url_launcher,
    const base::Closure& uninstall_callback) {
  DCHECK(frame != NULL);
  DCHECK(url_launcher != NULL);

  base::WeakPtr<TurndownPromptWindow> instance(
      (new TurndownPromptWindow(frame, url_launcher, uninstall_callback))
          ->weak_ptr_factory_.GetWeakPtr());

  DCHECK(!instance->IsWindow());

  if (instance->Create(frame->GetFrameWindow()) == NULL) {
    DPLOG(ERROR) << "Failed to create HWND for TurndownPromptWindow.";
    return base::WeakPtr<TurndownPromptWindow>();
  }

  // Subclass the "Learn more." text to make it behave like a link. Clicks are
  // routed to OnLearnMore().
  CWindow rte = instance->GetDlgItem(IDC_TD_PROMPT_LINK);
  instance->link_.reset(new CHyperLink());
  instance->link_->SubclassWindow(rte);
  instance->link_->SetHyperLinkExtendedStyle(HLINK_NOTIFYBUTTON,
                                             HLINK_NOTIFYBUTTON);

  SetupBitmapButton(instance.get());

  // Substitute the proper text given the current IE version.
  CWindow text = instance->GetDlgItem(IDC_TD_PROMPT_MESSAGE);
  string16 prompt_text(GetPromptText());
  if (!prompt_text.empty())
    text.SetWindowText(prompt_text.c_str());

  return instance;
}

TurndownPromptWindow::TurndownPromptWindow(
    InfobarContent::Frame* frame,
    UrlLauncher* url_launcher,
    const base::Closure& uninstall_closure)
    : frame_(frame),
      url_launcher_(url_launcher),
      uninstall_closure_(uninstall_closure),
      weak_ptr_factory_(this) {
}

TurndownPromptWindow::~TurndownPromptWindow() {}

// static
void TurndownPromptWindow::SetupBitmapButton(TurndownPromptWindow* instance) {
  DCHECK(instance);
  CWindow close_window = instance->GetDlgItem(IDDISMISS);
  instance->close_button_.reset(new CFBitmapButton());

  // Set the resource instance to the current dll which contains the bitmap.
  HINSTANCE old_res_module = _AtlBaseModule.GetResourceInstance();
  HINSTANCE this_module = _AtlBaseModule.GetModuleInstance();
  _AtlBaseModule.SetResourceInstance(this_module);

  HBITMAP close_bitmap = static_cast<HBITMAP>(
      LoadImage(this_module, MAKEINTRESOURCE(IDB_TURNDOWN_PROMPT_CLOSE_BUTTON),
                IMAGE_BITMAP, 0, 0, 0));

  // Restore the module's resource instance.
  _AtlBaseModule.SetResourceInstance(old_res_module);

  // Create the image list with the appropriate size and colour mask.
  instance->close_button_->m_ImageList.Create(kBitmapImageSize,
                                              kBitmapImageSize,
                                              ILC_COLOR8 | ILC_MASK, 4, 0);
  instance->close_button_->m_ImageList.Add(close_bitmap, RGB(255, 0, 255));
  instance->close_button_->m_ImageList.SetBkColor(CLR_NONE);

  // Free up the original bitmap.
  DeleteObject(close_bitmap);

  // Configure the button states and initialize the button.
  instance->close_button_->SetImages(0, 1, 2, 3);
  instance->close_button_->SubclassWindow(close_window);

  // The CDialogResize() implementation incorrectly captures the size
  // of the bitmap image button. Reset it here to ensure that resizing works
  // as desired.

  // Find the resize data. The parameters here must match the resize map in
  // turndown_prompt_window.h.
  _AtlDlgResizeData resize_params = { IDDISMISS, DLSZ_CENTER_Y | DLSZ_MOVE_X };
  int resize_index = instance->m_arrData.Find(resize_params);
  DCHECK(resize_index > -1 && resize_index < instance->m_arrData.GetSize());

  // Fiddle CDialogResize's internal data to fix up the size for the image
  // control.
  _AtlDlgResizeData& resize_data = instance->m_arrData[resize_index];
  resize_data.m_rect.right = resize_data.m_rect.left + kBitmapImageSize;
  resize_data.m_rect.top = 0;
  resize_data.m_rect.bottom = kBitmapImageSize;
}

void TurndownPromptWindow::OnFinalMessage(HWND) {
  delete this;
}

void TurndownPromptWindow::OnDestroy() {
  close_button_->m_ImageList.Destroy();
  frame_ = NULL;
}

BOOL TurndownPromptWindow::OnInitDialog(CWindow wndFocus, LPARAM lInitParam) {
  DlgResize_Init(false);  // false => 'no gripper'
  return TRUE;
}

LRESULT TurndownPromptWindow::OnLearnMore(WORD /*wParam*/,
                                          LPNMHDR /*lParam*/,
                                          BOOL& /*bHandled*/) {
  url_launcher_->LaunchUrl(SimpleResourceLoader::Get(
      IDS_CHROME_FRAME_TURNDOWN_LEARN_MORE_URL));
  return 0;
}

LRESULT TurndownPromptWindow::OnUninstall(WORD /*wNotifyCode*/,
                                          WORD /*wID*/,
                                          HWND /*hWndCtl*/,
                                          BOOL& /*bHandled*/) {
  frame_->CloseInfobar();
  if (!uninstall_closure_.is_null())
    uninstall_closure_.Run();
  return 0;
}

LRESULT TurndownPromptWindow::OnDismiss(WORD /*wNotifyCode*/,
                                        WORD /*wID*/,
                                        HWND /*hWndCtl*/,
                                        BOOL& /*bHandled*/) {
  frame_->CloseInfobar();
  return 0;
}

// static
string16 TurndownPromptWindow::GetPromptText() {
  IEVersion ie_version = GetIEVersion();
  int message_id = IDS_CHROME_FRAME_TURNDOWN_TEXT_IE_NEWER;
  if (ie_version == IE_6 || ie_version == IE_7 || ie_version == IE_8)
    message_id = IDS_CHROME_FRAME_TURNDOWN_TEXT_IE_OLDER;
  return SimpleResourceLoader::GetInstance()->Get(message_id);
}
