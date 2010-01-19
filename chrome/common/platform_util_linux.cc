// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/process_watcher.h"
#include "googleurl/src/gurl.h"

namespace {

void XDGOpen(const std::string& path) {
  std::vector<std::string> argv;
  argv.push_back("xdg-open");
  argv.push_back(path);

  base::environment_vector env;
  // xdg-open can fall back on mailcap which eventually might plumb through
  // to a command that needs a terminal.  Set the environment variable telling
  // it that we definitely don't have a terminal available and that it should
  // bring up a new terminal if necessary.  See "man mailcap".
  env.push_back(std::make_pair("MM_NOTTTY", "1"));

  // In Google Chrome, we do not let GNOME's bug-buddy intercept our crashes.
  // However, we do not want this environment variable to propagate to external
  // applications. See http://crbug.com/24120
  char* disable_gnome_bug_buddy = getenv("GNOME_DISABLE_CRASH_DIALOG");
  if (disable_gnome_bug_buddy &&
      disable_gnome_bug_buddy == std::string("SET_BY_GOOGLE_CHROME")) {
    env.push_back(std::make_pair("GNOME_DISABLE_CRASH_DIALOG", ""));
  }

  base::file_handle_mapping_vector no_files;
  base::ProcessHandle handle;
  if (base::LaunchApp(argv, env, no_files, false, &handle))
    ProcessWatcher::EnsureProcessGetsReaped(handle);
}

}  // namespace

namespace platform_util {

// TODO(estade): It would be nice to be able to select the file in the file
// manager, but that probably requires extending xdg-open. For now just
// show the folder.
void ShowItemInFolder(const FilePath& full_path) {
  FilePath dir = full_path.DirName();
  if (!file_util::DirectoryExists(dir))
    return;

  XDGOpen(dir.value());
}

void OpenItem(const FilePath& full_path) {
  XDGOpen(full_path.value());
}

void OpenExternal(const GURL& url) {
  XDGOpen(url.spec());
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  // A detached widget won't have a toplevel window as an ancestor, so we can't
  // assume that the query for toplevel will return a window.
  GtkWidget* toplevel = gtk_widget_get_ancestor(view, GTK_TYPE_WINDOW);
  return GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
}

string16 GetWindowTitle(gfx::NativeWindow window) {
  const gchar* title = gtk_window_get_title(window);
  return UTF8ToUTF16(title);
}

bool IsWindowActive(gfx::NativeWindow window) {
  return gtk_window_is_active(window);
}

bool IsVisible(gfx::NativeView view) {
  return GTK_WIDGET_VISIBLE(view);
}

void SimpleErrorBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  GtkWidget* dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", UTF16ToUTF8(message).c_str());
  gtk_util::ApplyMessageDialogQuirks(dialog);
  gtk_window_set_title(GTK_WINDOW(dialog), UTF16ToUTF8(title).c_str());
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show_all(dialog);
}

/* Warning: this may be either Linux or ChromeOS */
string16 GetVersionStringModifier() {
  char* env = getenv("CHROME_VERSION_EXTRA");
  if (!env)
    return string16();
  std::string modifier(env);

#if defined(GOOGLE_CHROME_BUILD)
  // Only ever return "", "unknown", "dev" or "beta" in a branded build.
  if (modifier == "unstable")  // linux version of "dev"
    modifier = "dev";
  if (modifier == "stable") {
    modifier = "";
  } else if ((modifier == "dev") || (modifier == "beta")) {
    // do nothing.
  } else {
    modifier = "unknown";
  }
#endif

  return ASCIIToUTF16(modifier);
}

}  // namespace platform_util
