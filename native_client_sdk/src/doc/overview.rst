.. _overview:

##################
Technical Overview
##################

.. contents::
  :local:
  :backlinks: none
  :depth: 2

Introduction
============

**Native Client** (NaCl) is an open-source technology for running native
applications in the browser, with the goal of maintaining the portability
and safety that people expect from web applications. Native Client expands web
programming beyond JavaScript, enabling developers to enhance their web
applications using their preferred language. This document describes a few of
the key benefits of Native Client, as well as current limitations and common use
cases.

Google has implemented the open-source `Native Client project
<http://www.chromium.org/nativeclient>`_ in the Chrome browser on Windows, Mac,
Linux, and Chrome OS. The :doc:`Native Client Software Development Kit (SDK)
<sdk/index>`, itself an open-source project, lets developers create web
applications that use NaCl and run in Chrome across OS platforms.

A web application that uses NaCl generally consists of a combination of
JavaScript, HTML, CSS, and a NaCl module that is written in a language supported
by the SDK. The NaCl SDK currently supports C and C++. As compilers for
additional languages are developed, the SDK will be updated to support those
languages as well.

.. image:: /images/web-app-with-nacl.png

Why use Native Client?
======================

The Native Client open-source technology is designed to run compiled code
securely inside a browser at near-native speeds. Native Client puts web
applications on the same playing field as traditional (locally-run) software; it
provides the means to fully harness the system's computational resources for
applications like 3D games and multimedia editors. Native Client also aims to
give C and C++ (and eventually other languages) the same level of portability
and safety that JavaScript provides on the web today.

Here are some of the key features that Native Client offers:

* **Graphics, audio, and much more:** Run native code modules that render 2D
  and 3D graphics, play audio, respond to mouse and keyboard events, run on
  multiple threads, and access memory directly---all without requiring
  the user to install a plugin.
* **Portability:** Write your applications once and you'll be able to run them
  across operating systems (Windows, Linux, Mac, and Chrome OS) and CPU
  architectures (x86, ARM).
* **Easy migration path to the web:** Many developers and companies have years
  of work invested in existing desktop applications. Native Client makes the
  transition from the desktop to a web application significantly easier because
  it supports C and C++.
* **Security:** Native Client uses a double sandbox model designed to protect
  the user's system from malicious or buggy applications. This model offers the
  safety of traditional web applications without sacrificing performance and
  without requiring users to install a plugin.
* **Performance:** Native Client allows your application to run at a speed
  comparable to a desktop app (within 5-15% of native speed); it also allows
  harnessing all available CPU cores via a threading API. This capability
  enables demanding applications such as console-quality games to run inside the
  browser.

Common use cases
================

Typical use cases for Native Client include the following:

* **Existing software components:** With its C and C++ language support, Native
  Client enables you to reuse current software modules in a web
  application---you don't need to spend time reinventing and debugging code
  that's already proven to work well.
* **Legacy desktop applications:** Native Client provides a smooth migration
  path from desktop applications to the web. You can port and recompile existing
  code for the computation engine of your application directly to Native Client,
  and need repurpose only the user interface and event handling portions to the
  new browser platform. Native Client allows you to embed existing functionality
  directly into the browser. At the same time, your application takes advantage
  of things the browser does well: handling user interaction and processing
  events, based on the latest developments in HTML5.
* **Enterprise applications that require heavy computation:** Native Client
  handles the number crunching required by large-scale enterprise applications.
  To ensure protection of user data, Native Client enables you to build complex
  cryptographic algorithms directly into the browser so that unencrypted data
  never goes out over the network.
* **Multimedia applications:** Codecs for processing sounds, images, and movies
  can be added to the browser in a Native Client module.
* **Games:** Native Client enables a web application to run close to native
  speed, reusing existing multithreaded/multicore C/C++ code bases, combined
  with low-level access to low-latency audio, networking APIs and OpenGL ES with
  programmable shaders. Programming to Native Client also enables your binary to
  run unchanged across many platforms. Native Client is a natural fit for
  running a physics engine or artificial intelligence module that powers a
  sophisticated web game.
* **Any application that requires acceleration**: Native Client fits seamlessly
  into any web application, so it is up to the developer to decide to what
  extent to utilize it. Native Client usage covers the full spectrum from
  complete applications to small optimized routines that accelerate vital parts
  of traditional web applications.

How Native Client works
=======================

Native Client is an umbrella name for a set of interrelated software components
that work together to provide a way to develop C/C++ applications and run them
securely on the web.

At a high level, Native Client consists of:

* **Toolchains**: collections of development tools (compilers, linkers, etc.)
  that transform C/C++ code to Native Client modules.
* **Runtime components**: embedded in the browser (or another host platform),
  that allow to execute Native Client modules securely and efficiently.

The following diagram shows how these components interact:

.. image:: /images/nacl-pnacl-component-diagram.png

Native Client executables and sandbox
-------------------------------------

Since Native Client permits executing native code on the client's machine,
special security measures have to be implemented. The security is achieved
through the following means:

* The NaCl sandbox ensures that code accesses system resources only through
  safe, whitelisted APIs, and operates within its limits without attempting to
  interfere with other code running within the browser or outside it.
* The NaCl validator is used prior to running native code to statically analyze
  the code and make sure it only uses allowed and safe code and data patterns.

These come in addition to the existing sandbox in the browser---the Native
Client module always executes within a process with restricted permissions. The
only interaction of this process with the outside world is through sanctioned
browser interfaces. For this reason, we say that Native Client employs a *double
sandbox* design.

PNaCl and NaCl
--------------

*PNaCl* (Portable Native Client) employs state-of-the-art compiler technology to
compile C/C++ source code to a portable bitcode executable (**pexe**). PNaCl
bitcode is an OS- and architecture-independent format that can be freely
distributed on the web and :ref:`embedded in web
applications<link_nacl_in_web_apps>`.

The *PNaCl translator* is a component embedded in the web browser; its task is
to run a **pexe**. Internally, the translator compiles a **pexe** to a **nexe**
(a native executable for the host platform's architecture) and then executes it
within the Native Client sandbox as described above. It also uses intelligent
caching to avoid this compilation if this **pexe** was already compiled on the
client's browser.

Native Client also supports running a **nexe** directly in the browser. However,
since a **nexe** contains architecture-specific machine code, distributing
**nexe** modules on the open web is not allowed. **nexe** modules can only be
used as part of applications that are installed from the Chrome Web Store and in
browser extensions.

Toolchains
----------

A *toolchain* is a set of tools used to create an application from a set of
source files. In the case of Native Client, a toolchain consists of a compiler,
linker, assembler and other tools that are used by the developer to convert an
application written in C/C++ into a module loadable by the browser.

The Native Client SDK provides two toolchains:

* A PNaCl toolchain for generating portable NaCl modules (**pexe**).
* A gcc-based toolchain (**nacl-gcc**) for generating native NaCl modules
  (**nexe**).

For most applications, the PNaCl toolchain is recommended. The **nacl-gcc**
toolchain should only be used if the application will not be available on the
open web.

.. _link_nacl_in_web_apps:

Native Client in a web application
==================================

A Native Client application consists of a set of files:

* **HTML web page**, **CSS**, and **JavaScript** files, as in any modern web
  application. The JavaScript is also responsible for communicating with the
  NaCl module.
* The **pexe** (portable NaCl module). This module uses the :ref:`Pepper
  <link_pepper>` API, which provides the bridge to JavaScript and
  browser resources.
* A **manifest** file that specifies the **pexe** to load with some loading
  options. This manifest file is embedded into the HTML page through an
  ``<embed>`` tag.

.. image:: /images/nacl-in-a-web-app.png

For more details, see TODO (link to example walk-through).

.. _link_pepper:

Pepper Plugin API
-----------------

The Pepper Plugin API (PPAPI), called **Pepper** for convenience, is an
open-source, cross-platform C/C++ API for web browser plugins. From the point
of view of NaCl, Pepper allows a C/C++ NaCl module to communicate with the
hosting browser and get access to system-level functions in a safe and portable
way. One of the security constraints in NaCl is that modules cannot make any
OS-level calls directly. Pepper provides analogous APIs that modules can target
instead.

You can use the Pepper APIs to gain access to the full array of browser
capabilities, including:

* :doc:`Talk to the JavaScript code in your application
  <devguide/coding/message-system>` from the C++ code in your NaCl module.
* :doc:`Do file I/O <devguide/coding/FileIO>`.
* :doc:`Play audio <devguide/coding/audio>`.
* :doc:`Render 3D graphics <devguide/coding/3D-graphics>`.

Pepper includes both a C API and a C++ API. The C++ API is a set of bindings
written on top of the C API. For additional information about Pepper, see
`Pepper Concepts <http://code.google.com/p/ppapi/wiki/Concepts>`_.

Versioning
==========

Chrome releases on a six week cycle, and developer versions of Chrome are
pushed to the public beta channel three weeks before release. As with any
software, each release of Chrome includes changes to Native Client and the
Pepper interfaces that may require modification to existing applications.
However, modules compiled for one version of Pepper/Chrome should generally
work with subsequent versions of Pepper/Chrome. The SDK includes multiple
`versions <https://developers.google.com/native-client/version>`_ of the Pepper
APIs to help developers make adjustments to API changes and take advantage of
new features.

Where to go next
================

The :doc:`quick start <quick-start>` document provides links to downloads and
documentation that should help you get started with developing and distributing
NaCl applications.

