// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window_testing.h"

#include "base/lazy_instance.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace {

typedef std::vector<DevToolsWindowTesting*> DevToolsWindowTestings;
base::LazyInstance<DevToolsWindowTestings>::Leaky g_instances =
    LAZY_INSTANCE_INITIALIZER;

}

DevToolsWindowTesting::DevToolsWindowTesting(DevToolsWindow* window)
    : devtools_window_(window) {
  DCHECK(window);
  window->close_callback_ =
      base::Bind(&DevToolsWindowTesting::WindowClosed, window);
  g_instances.Get().push_back(this);
}

DevToolsWindowTesting::~DevToolsWindowTesting() {
  DevToolsWindowTestings* instances = g_instances.Pointer();
  DevToolsWindowTestings::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);
  if (!close_callback_.is_null()) {
    close_callback_.Run();
    close_callback_ = base::Closure();
  }
}

// static
DevToolsWindowTesting* DevToolsWindowTesting::Get(DevToolsWindow* window) {
  DevToolsWindowTesting* testing = DevToolsWindowTesting::Find(window);
  if (!testing)
    testing = new DevToolsWindowTesting(window);
  return testing;
}

// static
DevToolsWindowTesting* DevToolsWindowTesting::Find(DevToolsWindow* window) {
  if (g_instances == NULL)
    return NULL;
  DevToolsWindowTestings* instances = g_instances.Pointer();
  for (DevToolsWindowTestings::iterator it(instances->begin());
       it != instances->end();
       ++it) {
    if ((*it)->devtools_window_ == window)
      return *it;
  }
  return NULL;
}

Browser* DevToolsWindowTesting::browser() {
  return devtools_window_->browser_;
}

content::WebContents* DevToolsWindowTesting::main_web_contents() {
  return devtools_window_->main_web_contents_;
}

content::WebContents* DevToolsWindowTesting::toolbox_web_contents() {
  return devtools_window_->toolbox_web_contents_;
}

void DevToolsWindowTesting::SetInspectedPageBounds(const gfx::Rect& bounds) {
  devtools_window_->SetInspectedPageBounds(bounds);
}

void DevToolsWindowTesting::SetCloseCallback(const base::Closure& closure) {
  close_callback_ = closure;
}

// static
void DevToolsWindowTesting::WindowClosed(DevToolsWindow* window) {
  DevToolsWindowTesting* testing = DevToolsWindowTesting::Find(window);
  if (testing)
    delete testing;
}

// static
void DevToolsWindowTesting::WaitForDevToolsWindowLoad(DevToolsWindow* window) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  window->SetLoadCompletedCallback(runner->QuitClosure());
  runner->Run();
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowSync(
    content::WebContents* inspected_web_contents,
    bool is_docked) {
  std::string settings = is_docked ?
      "{\"currentDockState\":\"\\\"bottom\\\"\"}" :
      "{\"currentDockState\":\"\\\"undocked\\\"\"}";
  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(inspected_web_contents));
  DevToolsWindow::ToggleDevToolsWindow(
        inspected_web_contents, true, DevToolsToggleAction::Show(), settings);
  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent.get());
  WaitForDevToolsWindowLoad(window);
  return window;
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowSync(
    Browser* browser,
    bool is_docked) {
  return OpenDevToolsWindowSync(
      browser->tab_strip_model()->GetActiveWebContents(), is_docked);
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowForWorkerSync(
    Profile* profile, content::DevToolsAgentHost* worker_agent) {
  DevToolsWindow::OpenDevToolsWindowForWorker(
      profile, worker_agent);
  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(worker_agent);
  WaitForDevToolsWindowLoad(window);
  return window;
}

// static
void DevToolsWindowTesting::CloseDevToolsWindow(
    DevToolsWindow* window) {
  if (window->is_docked_) {
    window->CloseWindow();
  } else {
    window->browser_->window()->Close();
  }
}

// static
void DevToolsWindowTesting::CloseDevToolsWindowSync(
    DevToolsWindow* window) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  DevToolsWindowTesting::Get(window)->SetCloseCallback(runner->QuitClosure());
  CloseDevToolsWindow(window);
  runner->Run();
}

// DevToolsWindowCreationObserver ---------------------------------------------

DevToolsWindowCreationObserver::DevToolsWindowCreationObserver()
    : devtools_window_(nullptr) {
  DevToolsWindow::AddCreationCallbackForTest(base::Bind(
      &DevToolsWindowCreationObserver::DevToolsWindowCreated,
      base::Unretained(this)));
}

DevToolsWindowCreationObserver::~DevToolsWindowCreationObserver() {
}

void DevToolsWindowCreationObserver::Wait() {
  if (devtools_window_)
    return;
  runner_ = new content::MessageLoopRunner();
  runner_->Run();
}

void DevToolsWindowCreationObserver::WaitForLoad() {
  Wait();
  if (devtools_window_)
    DevToolsWindowTesting::WaitForDevToolsWindowLoad(devtools_window_);
}

void DevToolsWindowCreationObserver::DevToolsWindowCreated(
    DevToolsWindow* devtools_window) {
  devtools_window_ = devtools_window;
  if (runner_.get())
      runner_->QuitClosure().Run();
}
