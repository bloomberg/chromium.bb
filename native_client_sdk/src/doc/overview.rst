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
compiled code in the browser, with the goal of maintaining the portability
and safety that users expect from web applications. Native Client expands web
programming beyond JavaScript, enabling developers to enhance their web
applications using their preferred language. This document describes some of
the key benefits and common use cases of Native Client.

Google has implemented the open-source `Native Client project
<http://www.chromium.org/nativeclient>`_ in the Chrome browser on Windows, Mac,
Linux, and Chrome OS. The :doc:`Native Client Software Development Kit (SDK)
<sdk/download>`, itself an open-source project, lets developers create web
applications that use NaCl and run in Chrome across multiple platforms.

A web application that uses Native Client generally consists of a combination of
JavaScript, HTML, CSS, and a NaCl module that is written in a language supported
by the SDK. The NaCl SDK currently supports C and C++; as compilers for
additional languages are developed, the SDK will be updated to support those
languages as well.

.. image:: /images/web-app-with-nacl.png

Why use Native Client?
======================

Native Client open-source technology is designed to run compiled code
securely inside a browser at near-native speeds. Native Client puts web
applications on the same playing field as traditional (locally-run)
software---it provides the means to fully harness the client's computational
resources for applications such as 3D games, multimedia editors, CAD modeling,
client-side data analytics, and interactive simulations. 
Native Client also aims to give C and C++ (and eventually other languages) the
same level of portability and safety that JavaScript provides on the web today.

Here are a few of the key benefits that Native Client offers:

* **Graphics, audio, and much more:** Run native code modules that render 2D
  and 3D graphics, play audio, respond to mouse and keyboard events, run on
  multiple threads, and access memory directly---all without requiring
  the user to install a plugin.
* **Portability:** Write your applications once and you'll be able to run them
  across operating systems (Windows, Linux, Mac, and Chrome OS) and CPU
  architectures (x86 and ARM).
* **Easy migration path to the web:** Many developers and companies have years
  of work invested in existing desktop applications. Native Client makes the
  transition from the desktop to a web application significantly easier because
  it supports C and C++.
* **Security:** Native Client uses a double sandbox model designed to protect
  the user's system from malicious or buggy applications. This model offers the
  safety of traditional web applications without sacrificing performance and
  without requiring users to install a plugin.
* **Performance:** Native Client allows web applications to run at speeds
  comparable to desktop applications (within 5-15% of native speed).
  Native Client also allows applications to harness all available CPU cores via
  a threading API; this enables demanding applications such as console-quality
  games to run inside the browser.

Common use cases
================

Typical use cases for Native Client include the following:

* **Existing software components:** With support for C and C++, Native
  Client enables you to reuse existing software modules in
  web applications---you don't need to rewrite and debug code
  that's already proven to work well.
* **Legacy desktop applications:** Native Client provides a smooth migration
  path from desktop applications to the web. You can port and recompile existing
  code for the computation engine of your application directly to Native Client,
  and need repurpose only the user interface and event handling portions to the
  new browser platform. Native Client allows you to embed existing functionality
  directly into the browser. At the same time, your application can take
  advantage of things the browser does well: handling user interaction and
  processing events, based on the latest developments in HTML5.
* **Heavy computation in enterprise applications:** Native Client can handle the
  number crunching required by large-scale enterprise applications. To ensure
  protection of user data, Native Client enables you to build complex
  cryptographic algorithms directly into the browser so that unencrypted data
  never goes out over the network.
* **Multimedia applications:** Codecs for processing sounds, images, and movies
  can be added to the browser in a Native Client module.
* **Games:** Native Client lets web applications run at close to native
  speed, reuse existing multithreaded/multicore C/C++ code bases, and
  access low-latency audio, networking APIs, and OpenGL ES with programmable
  shaders. Native Client is a natural fit for running a physics engine or
  artificial intelligence module that powers a sophisticated web game.
  Native Client also enables applications to run unchanged across
  many platforms.
* **Any application that requires acceleration**: Native Client fits seamlessly
  into web applications---it's up to you to decide to what extent to use it.
  Use of Native Client covers the full spectrum from complete applications to
  small optimized routines that accelerate vital parts of web apps.

.. _link_how_nacl_works:

How Native Client works
=======================

Native Client is an umbrella name for a set of interrelated software components
that work together to provide a way to develop C/C++ applications and run them
securely on the web.

At a high level, Native Client consists of:

* **Toolchains**: collections of development tools (compilers, linkers, etc.)
  that transform C/C++ code to Native Client modules.
* **Runtime components**: components embedded in the browser or other
  host platforms that allow execution of  Native Client modules
  securely and efficiently.

The following diagram shows how these components interact:

.. image:: /images/nacl-pnacl-component-diagram.png

The left side of the diagram shows how to use Portable Native Client
(PNaCl, pronounced "pinnacle"). Developers use the PNaCl toolchain
to produce a single, portable (**pexe**) module. At runtime, a translator
built into the browser translates the pexe into native code for the
relevant client architecture. 

The right side of the diagram shows how to use traditional (non-portable)
Native Client. Developers use a nacl-gcc based toolchain to produce multiple
architecture-dependent (**nexe**) modules, which are packaged into an
application. At runtime, the browser decides which nexe to load based
on the architecture of the client machine.

Security
--------

Since Native Client permits the execution of native code on client machines,
special security measures have to be implemented:

* The NaCl sandbox ensures that code accesses system resources only through
  safe, whitelisted APIs, and operates within its limits without attempting to
  interfere with other code running either within the browser or outside it.
* The NaCl validator statically analyzes code prior to running it
  to make sure it only uses code and data patterns that are permitted and safe.

The above security measures are in addition to the existing sandbox in the
Chrome browser---the Native Client module always executes in a process with
restricted permissions. The only interaction between this process and the
outside world is through sanctioned browser interfaces. Because of the
combination of the NaCl sandbox and the Chrome sandbox, we say that
Native Client employs a double sandbox design.

Portability
-----------

Portable Native Client (PNaCl, prounounced "pinnacle") employs state-of-the-art
compiler technology to compile C/C++ source code to a portable bitcode
executable (**pexe**). PNaCl bitcode is an OS- and architecture-independent
format that can be freely distributed on the web and :ref:`embedded in web
applications<link_nacl_in_web_apps>`.

The PNaCl translator is a component embedded in the Chrome browser; its task is
to run pexe modules. Internally, the translator compiles a pexe to a nexe
(a native executable for the client platform's architecture), and then executes
the nexe within the Native Client sandbox as described above. It also uses
intelligent caching to avoid re-compiling the pexe if it was previously compiled
on the client's browser.

Native Client also supports the execution of nexe modules directly in the
browser. However, since nexes contain architecture-specific machine code,
they are not allowed to be distributed on the open web---they can only be
used as part of applications and extensions that are installed from the
Chrome Web Store.

For more details on the difference between NaCl and PNaCl, see
:doc:`NaCl and PNaCl <nacl-and-pnacl>`.

.. _toolchains:

Toolchains
----------

A toolchain is a set of tools used to create an application from a set of
source files. In the case of Native Client, a toolchain consists of a compiler,
linker, assembler and other tools that are used to convert an
application written in C/C++ into a module that is loadable by the browser.

The Native Client SDK provides two toolchains:

* a **PNaCl toolchain** for generating portable NaCl modules (pexe files)
* a **gcc-based toolchain (nacl-gcc)** for generating non-portable NaCl modules
  (nexe files)

The PNaCl toolchain is recommended for most applications. The nacl-gcc
toolchain should only be used for applications that will not be distributed
on the open web.

.. _link_nacl_in_web_apps:

Native Client in a web application
==================================

.. _application_files:

A Native Client application consists of a set of files:

* **HTML**, **CSS**, and **JavaScript** files, as in any modern web
  application. The JavaScript code is responsible for communicating with the
  NaCl module.
* A **pexe** (portable NaCl) file. This module uses the :ref:`Pepper
  <link_pepper>` API, which provides the bridge to JavaScript and
  browser resources.
* A Native Client **manifest** file that specifies the pexe to load, along with
  some loading options. This manifest file is embedded into the HTML page
  through an ``<embed>`` tag, as shown in the figure below.

.. image:: /images/nacl-in-a-web-app.png

For more details, see :doc:`Application Structure
<devguide/coding/application-structure>`.

.. _link_pepper:

Pepper Plugin API
-----------------

The Pepper Plugin API (PPAPI), called **Pepper** for convenience, is an
open-source, cross-platform C/C++ API for web browser plugins. From the point
of view of Native Client, Pepper allows a C/C++ module to communicate with
the hosting browser and get access to system-level functions in a safe and
portable way. One of the security constraints in Native Client is that modules
cannot make any OS-level calls directly. Pepper provides analogous APIs that
modules can target instead.

You can use the Pepper APIs to gain access to the full array of browser
capabilities, including:

* :doc:`Talking to the JavaScript code in your application
  <devguide/coding/message-system>` from the C++ code in your NaCl module.
* :doc:`Doing file I/O <devguide/coding/file-io>`.
* :doc:`Playing audio <devguide/coding/audio>`.
* :doc:`Rendering 3D graphics <devguide/coding/3D-graphics>`.

Pepper includes both a C API and a C++ API. The C++ API is a set of bindings
written on top of the C API. For additional information about Pepper, see
`Pepper Concepts <http://code.google.com/p/ppapi/wiki/Concepts>`_.

Versioning
==========

Chrome is released on a six week cycle, and developer versions of Chrome are
pushed to the public beta channel three weeks before each release. As with any
software, each release of Chrome may include changes to Native Client and the
Pepper interfaces that may require modification to existing applications.
However, modules compiled for one version of Pepper/Chrome should work with
subsequent versions of Pepper/Chrome. The SDK includes multiple versions of the
Pepper APIs to help developers make adjustments to API changes and take
advantage of new features: `stable </native-client/pepper_stable>`_, `beta
</native-client/pepper_beta>`_ and `dev </native-client/pepper_dev>`_.

Where to start
==============

The :doc:`Quick Start <quick-start>` document provides links to downloads and
documentation that should help you get started with developing and distributing
Native Client applications.
