{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'mouselock',
      'TYPE' : 'main',
      'SOURCES' : ['mouselock.cc', 'mouselock.h'],
    }
  ],
  'DATA': ['check_browser.js'],
  'DEST': 'examples',
  'NAME': 'mouselock',
  'TITLE': 'Mouse Lock',
  'DESC': """
The Mouselock example demonstrates how to use the MouseLock API to hide
the mouse cursor.  Mouse lock is only available in full-screen mode.  You can
lock and unlock the mouse while in full-screen mode by pressing the Enter key.
""",
  'INFO': 'Teaching focus: Mouse lock, Full-screen.'
}

