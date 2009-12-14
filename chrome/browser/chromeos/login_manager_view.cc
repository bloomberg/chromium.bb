// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login_manager_view.h"

#include "app/resource_bundle.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/browser/chromeos/image_background.h"
#include "chrome/common/chrome_switches.h"
#include "grit/theme_resources.h"
#include "views/controls/label.h"
#include "views/focus/accelerator_handler.h"
#include "views/grid_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using views::Background;
using views::ColumnSet;
using views::GridLayout;
using views::Label;
using views::Textfield;
using views::View;
using views::Widget;
using views::Accelerator;

const int kPanelY = 0;
const int kUsernameY = 386;
const int kPanelSpacing = 36;
const int kTextfieldWidth = 286;
const int kBottomPadding = 112;

namespace browser {

bool EmitLoginPromptReady() {
  base::ProcessHandle handle;
  std::vector<std::string> argv;
  argv.push_back("/opt/google/chrome/emit_login_prompt_ready");
  base::environment_vector no_env;
  base::file_handle_mapping_vector no_files;
  return base::LaunchApp(argv, no_env, no_files, false, &handle);
}

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
void ShowLoginManager() {
  views::Window::CreateChromeWindow(NULL,
                                    gfx::Rect(),
                                    new LoginManagerView())->Show();
  EmitLoginPromptReady();
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}
}  // namespace browser

LoginManagerView::LoginManagerView() {
  Init();
}

LoginManagerView::~LoginManagerView() {
  MessageLoop::current()->Quit();
}


void LoginManagerView::Init() {
  username_field_ = new views::Textfield;
  password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);

  // Creates the main window
  BuildWindow();

  // Controller to handle events from textfields
  username_field_->SetController(this);
  password_field_->SetController(this);
}

gfx::Size LoginManagerView::GetPreferredSize() {
  return dialog_dimensions_;
}


void LoginManagerView::BuildWindow() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  panel_pixbuf_ = rb.GetPixbufNamed(IDR_LOGIN_PANEL);
  background_pixbuf_ = rb.GetPixbufNamed(IDR_LOGIN_BACKGROUND);

  // --------------------- Get attributes of images -----------------------
  dialog_dimensions_.SetSize(gdk_pixbuf_get_width(background_pixbuf_),
                             gdk_pixbuf_get_height(background_pixbuf_));

  int panel_height = gdk_pixbuf_get_height(panel_pixbuf_);
  int panel_width = gdk_pixbuf_get_width(panel_pixbuf_);

  // ---------------------- Set up root View ------------------------------
  set_background(new views::ImageBackground(background_pixbuf_));

  // Set layout
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(1, 0);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::FILL, 0,
                        GridLayout::FIXED, panel_width, panel_width);
  column_set->AddPaddingColumn(1, 0);

  // Row is resized with window (panel page)
  layout->AddPaddingRow(0, kPanelY);

  layout->StartRow(1, 0);
  {
    // Create login_prompt view
    View* login_prompt = new View();
    // TODO(cmasone): determine if it's more performant to do the background
    // by creating an BackgroundPainter directly, like so:
    // Background::CreateBackgroundPainter(true,
    //     Painter::CreateImagePainter(image, ...))
    login_prompt->set_background(new views::ImageBackground(panel_pixbuf_));

    // Set layout
    GridLayout* prompt_layout = new GridLayout(login_prompt);
    login_prompt->SetLayoutManager(prompt_layout);
    ColumnSet* prompt_column_set = prompt_layout->AddColumnSet(0);
    prompt_column_set->AddPaddingColumn(1, 0);
    prompt_column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                                 GridLayout::FIXED,
                                 kTextfieldWidth, kTextfieldWidth);
    prompt_column_set->AddPaddingColumn(1, 0);

    prompt_layout->AddPaddingRow(0, kUsernameY);
    prompt_layout->StartRow(1, 0);
    prompt_layout->AddView(username_field_);
    prompt_layout->AddPaddingRow(0, kPanelSpacing);
    prompt_layout->StartRow(1, 0);
    prompt_layout->AddView(password_field_);
    prompt_layout->AddPaddingRow(0, kBottomPadding);

    layout->AddView(login_prompt, 1, 1, GridLayout::CENTER, GridLayout::CENTER,
                    panel_width, panel_height);
  }

  layout->AddPaddingRow(1, 0);
}

views::View* LoginManagerView::GetContentsView() {
  return this;
}

bool LoginManagerView::Authenticate(const std::string& username,
                                    const std::string& password) {
  base::ProcessHandle handle;
  std::vector<std::string> argv;
  // TODO(cmasone): we'll want this to be configurable.
  argv.push_back("/opt/google/chrome/session");
  argv.push_back(username);
  argv.push_back(password);

  base::environment_vector no_env;
  base::file_handle_mapping_vector no_files;
  base::LaunchApp(argv, no_env, no_files, false, &handle);
  int child_exit_code;
  return base::WaitForExitCode(handle, &child_exit_code) &&
         child_exit_code == 0;
}

bool LoginManagerView::RunWindowManager(const std::string& username) {
  base::ProcessHandle handle;
  std::vector<std::string> argv;
  // TODO(cmasone): we'll want this to be configurable.
  argv.push_back("/etc/init.d/start_wm.sh");
  base::environment_vector env;

  // TODO(cmasone): This is a legacy way of communication the logged in user's
  //                email to xscreensaver.  Figure out a better way.
  env.push_back(std::pair<std::string, std::string>("CHROMEOS_USER", username));
  base::file_handle_mapping_vector no_files;
  return base::LaunchApp(argv, env, no_files, false, &handle);
}

void LoginManagerView::SetupSession(const std::string& username) {
  if (window()) {
    window()->Close();
  }
  if (username.find("@google.com") != std::string::npos) {
    // This isn't thread-safe.  However, the login window is specifically
    // supposed to be run in a blocking fashion, before any other threads are
    // created by the initial browser process.
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAutoSSLClientAuth);
  }
  RunWindowManager(username);
}

bool LoginManagerView::HandleKeystroke(views::Textfield* s,
    const views::Textfield::Keystroke& keystroke) {
  if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    // Disallow 0 size username | passwords
    if (username_field_->text().length() == 0 ||
        password_field_->text().length() == 0) {
      // Return true so that processing ends
      return true;
    } else {
      // Set up credentials to prepare for authentication.  Also
      // perform wstring to string conversion
      std::string username, password;
      username.assign(username_field_->text().begin(),
                      username_field_->text().end());
      password.assign(password_field_->text().begin(),
                      password_field_->text().end());

      if (!Authenticate(username, password)) {
        return false;
      }
      // TODO(cmasone): something sensible if errors occur.

      SetupSession(username);
      // Return true so that processing ends
      return true;
    }
  }
  // Return false so that processing does not end
  return false;
}


