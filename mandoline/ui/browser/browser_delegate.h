// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_DELEGATE_H_
#define MANDOLINE_UI_BROWSER_BROWSER_DELEGATE_H_

namespace mojo {
class View;
}

namespace mandoline {

class Browser;

// A BrowserDelegate is an interface to be implemented by an object that manages
// the lifetime of a Browser.
class BrowserDelegate {
 public:
  // Invoken when the Browser wishes to close. This gives the delegate the
  // opportunity to perform some cleanup.
  virtual void BrowserClosed(Browser* browser) = 0;

  // Requests initialization of state to display the browser on screen. Returns
  // whether UI was initialized at this point.
  virtual bool InitUIIfNecessary(Browser* browser, mojo::View* root_view) = 0;

 protected:
  virtual ~BrowserDelegate() {}
};

}  // mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_DELEGATE_H_
