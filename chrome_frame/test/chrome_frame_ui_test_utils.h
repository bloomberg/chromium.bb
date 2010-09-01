// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_CHROME_FRAME_UI_TEST_UTILS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_UI_TEST_UTILS_H_

#include <oleacc.h>

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "chrome_frame/test/win_event_receiver.h"

namespace gfx {
class Rect;
}

namespace chrome_frame_test {

// Wrapper for MSAA objects. In MSAA, there are two types of objects. The first,
// called an object or full object, has its own IAccessible interface. The
// second, called a simple element, does not have its own IAccessible interface
// and cannot have children. Simple elements must be referenced by combination
// of the parent object and the element's id in MSAA. This class handles this
// distinction transparently to the client.
class AccObject : public base::RefCounted<AccObject> {
 public:
  typedef std::vector<scoped_refptr<AccObject> > RefCountedAccObjectVector;

  // Creates an AccObject with an IAccessible and child id. |accessible| must
  // not be NULL. |child_id| must always be CHILDID_SELF unless this AccObject
  // is a simple element.
  AccObject(IAccessible* accessible, int child_id = CHILDID_SELF);

  // Creates an AccObject corresponding to the given window. May return NULL
  // if there is not accessible object for the window. The client owns the
  // created AccObject.
  static AccObject* CreateFromWindow(HWND hwnd);

  // Creates an AccObject corresponding to the object that generated a
  // WinEvent. Returns NULL on failure.
  static AccObject* CreateFromEvent(HWND hwnd, LONG object_id, LONG child_id);

  // Creates an AccObject from querying the given IDispatch. May return NULL
  // if the object does not implement IAccessible. The client owns the created
  // AccObject.
  static AccObject* CreateFromDispatch(IDispatch* dispatch);

  // Creates an AccObject corresponding to the accessible object at the screen
  // coordinates given. Returns NULL on failure. The client owns the created
  // AccObject.
  // Note: This does not work in Chrome.
  static AccObject* CreateFromPoint(int x, int y);

  // Performs the default action on this object. Returns whether the action
  // performed successfully. Will cause test failure if unsuccessful.
  // Note: This does not work for selecting menu items if the desktop is locked.
  bool DoDefaultAction();

  // Posts a left click message at this object's center point to the window
  // containing this object.
  bool LeftClick();

  // Posts a right click message at this object's center point to the window
  // containing this object.
  bool RightClick();

  // Focuses this object. Returns whether the object receives focus. Will cause
  // test failure if the object is not focused.
  bool Focus();

  // Selects this object. Returns whether the object is now selected. Will cause
  // test failure if the object is not selected.
  bool Select();

  // Sets the value of the object. Will cause test failure if unsuccessful.
  bool SetValue(const std::wstring& value);

  // Gets the name of the object and returns true on success.
  bool GetName(std::wstring* name);

  // Gets the role text of the object and returns true on success.
  bool GetRoleText(std::wstring* role_text);

  // Gets the value of the object and returns true on success.
  bool GetValue(std::wstring* value);

  // Gets the state of the object and returns true on success.
  bool GetState(int* state);

  // Gets the location of the object in screen coordinates and returns true
  // on success.
  bool GetLocation(gfx::Rect* location);

  // Gets the location of the object relative to the containing window's
  // client rect.
  bool GetLocationInClient(gfx::Rect* location);

  // Gets the parent of the object. May return NULL.
  AccObject* GetParent();

  // Gets the children of this object and returns true on success. |objects|
  // will not be modified unless if the operation is successful.
  bool GetChildren(RefCountedAccObjectVector* objects);

  // Gets the number of children of this object and returns true on success.
  bool GetChildCount(int* child_count);

  // Gets the window containing this object and returns true on success. This
  // method will return false if the object is not contained within a window.
  bool GetWindow(HWND* window);

  // Gets the class name for the window containing this object and returns true
  // on success. If this object does not have a window, this will return false.
  bool GetWindowClassName(std::wstring* class_name);

  // Returns whether this object is a simple element.
  bool IsSimpleElement();

  // Returns whether the two AccObjects point to the same accessibility object.
  // |other| can safely be NULL.
  bool Equals(AccObject* other);

  // Returns a description of this object.
  std::wstring GetDescription();

  // Returns a description of this object and it's accessibility tree. This
  // description will be ended by a newline.
  std::wstring GetTree();

 private:
  friend class base::RefCounted<AccObject>;
  ~AccObject() {}

  // Helper method for posting mouse button messages at this object's location.
  bool PostMouseButtonMessages(int button_up, int button_down);

  ScopedComPtr<IAccessible> accessible_;
  ScopedVariant child_id_;

  DISALLOW_COPY_AND_ASSIGN(AccObject);
};

// Finds an accessibility object with properties that match the specified
// matching patterns. These patterns can include the standard * and ? wildcards.
class AccObjectMatcher {
 public:
  // Create a matcher from the given string. |matcher| should include matching
  // patterns for each property separated by colons. Matching patterns must
  // be specified from left to right in the following order:
  //   1) Name
  //   2) Role Text: A string representation of a Windows object role, which can
  //        be found by using the win32 GetRoleText function. E.g.,
  //        ROLE_SYSTEM_ALERT should be represented as 'alert', and
  //        ROLE_SYSTEM_MENUPOPUP should be represented as 'popup menu'.
  //   3) Value
  // Matching patterns can be blank, essentially equal to *.
  // Literal *, ?, and : characters can be escaped with a backslash.
  AccObjectMatcher(const std::wstring& name = L"",
                   const std::wstring& role_text = L"",
                   const std::wstring& value = L"");

  // Finds the first object which satisfies this matcher and sets as |match|.
  // This searches the accessibility tree (including |object| itself) of
  // |object| in a pre-order fasion. If no object is matched, |match| will be
  // NULL. Returns true if no error occured while trying to find a match. It is
  // possible to use this method to test for an object's non-existence.
  bool Find(AccObject* object, scoped_refptr<AccObject>* match) const;

  // Same as above except that it searches within the accessibility tree of the
  // given window, which must support the IAccessible interface.
  bool FindInWindow(HWND hwnd, scoped_refptr<AccObject>* match) const;

  // Returns whether |object| satisfies this matcher.
  bool DoesMatch(AccObject* object) const;

  // Return a description of the matcher, for debugging/logging purposes.
  std::wstring GetDescription() const;

 private:
  bool FindHelper(AccObject* object, scoped_refptr<AccObject>* match) const;

  std::wstring name_;
  std::wstring role_text_;
  std::wstring value_;
};

// Observes various accessibility events.
class AccEventObserver : public WinEventListener {
 public:
  AccEventObserver();
  virtual ~AccEventObserver();

  // Begins watching for a value changed event for an accessibility object that
  // satisfies |matcher|. Once the event occurs, this observer will stop
  // watching.
  void WatchForOneValueChange(const AccObjectMatcher& matcher);

  // Called when the DOM accessibility tree for the page is ready.
  virtual void OnAccDocLoad(HWND hwnd) = 0;

  // Called when an accessibility object value changes. Only called if the
  // observer was set to watch for it.
  virtual void OnAccValueChange(HWND hwnd, AccObject* object,
                                const std::wstring& new_value) = 0;

  // Called when a new menu is shown.
  virtual void OnMenuPopup(HWND hwnd) = 0;

 private:
  class EventHandler : public base::RefCounted<EventHandler> {
   public:
    EventHandler(AccEventObserver* observer);

    // Examines the given event and invokes the corresponding method of its
    // observer.
    void Handle(DWORD event, HWND hwnd, LONG object_id, LONG child_id);

    AccEventObserver* observer_;
  };

  // Overriden from WinEventListener.
  virtual void OnEventReceived(DWORD event, HWND hwnd, LONG object_id,
                               LONG child_id);

  scoped_refptr<EventHandler> event_handler_;
  bool is_watching_;
  AccObjectMatcher watching_for_matcher_;
  WinEventReceiver event_receiver_;
};

// Finds an AccObject from the given window that satisfied |matcher|.
// Will cause test failure in case of error or if no match is found in the
// accessibility tree of the specified window. Returns whether the object was
// found.
bool FindAccObjectInWindow(HWND hwnd, const AccObjectMatcher& matcher,
                           scoped_refptr<AccObject>* object);

// Writes the accessibility tree for the given window to standard out. Used for
// debugging/logging.
void DumpAccessibilityTreeForWindow(HWND hwnd);

// Returns whether the desktop is unlocked.
bool IsDesktopUnlocked();

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_UI_TEST_UTILS_H_
