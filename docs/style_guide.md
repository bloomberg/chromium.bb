# Open Screen Library Style Guide

The Open Screen Library follows the
[Chromium C++ coding style](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md).
We also follow the
[Chromium C++ Do's and Don'ts](https://sites.google.com/a/chromium.org/dev/developers/coding-style/cpp-dos-and-donts).

C++11 language and library features are allowed in the Open Screen Library
according to the
[C++11 use in Chromium](https://chromium-cpp.appspot.com#core-whitelist) guidelines.

## Copy and Move Operators

Use the following guidelines when deciding on copy and move semantics for
objects.

- Objects with data members greater than 32 bytes should be move-able.
- Known large objects (I/O buffers, etc.) should be be move-only.
- Application or client provided objects of variable length should be move-able
  (since they may be arbitrarily large in size) and, if possible, move-only.
- Inherently non-copyable objects (like sockets) should be move-only.

We [prefer the use of `default` and `delete`](https://sites.google.com/a/chromium.org/dev/developers/coding-style/cpp-dos-and-donts#TOC-Prefer-to-use-default)
to declare the copy and move semantics of objects.  See
[Stoustrop's C++ FAQ](http://www.stroustrup.com/C++11FAQ.html#default)
for details on how to do that.

## Disallowed styles and features

Blink style is *not allowed* anywhere in the Open Screen Library.

C++14-only features are currently *not allowed* in the Open Screen Library.
