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

**Native Client** is an open-source technology for running native compiled code
in the browser, with the goal of maintaining the OS portability and safety
that people expect from web apps. Native Client expands web programming
beyond JavaScript, enabling developers to enhance their web applications
using their preferred language. This document describes a few of the key
benefits of Native Client, as well as current limitations, common use cases,
and tips for getting started with Native Client.

Google has implemented the open-source
`Native Client project <http://www.chromium.org/nativeclient>`_ in the Chrome
browser on Windows, Mac, Linux, and Chrome OS. The **Native Client Software
Development Kit (SDK)**, itself an open-source project, lets developers create
web applications that use Native Client and run in Chrome across OS
platforms. In addition to software libraries, the SDK includes a toolchain
tailored to generate executable Native Client code, as well as a variety of
code examples and documentation.

A web application that uses Native Client generally consists of a combination
of JavaScript, HTML, CSS, and a Native Client module that is written in a
language supported by a Native Client compiler. The Native Client SDK
currently supports C and C++. As compilers for additional languages are
developed, the SDK will be updated to support those languages as well.

.. image:: /images/NaclBlock.png

Why use Native Client?
======================

The Native Client open-source technology is designed to run native compiled
code securely inside browsers. Native Client puts web applications on the same
playing field as local applications, providing the raw speed needed to compete
with traditional software like 3D games, video editing, and other applications.
Native Client also gives languages like C and C++ (and eventually others as
well) the same level of portability and safety that JavaScript provides on the
web today.

Here are some of the key features that Native Client offers:

* **Graphics, audio, and much more:** Run native code modules that render 2D
  and 3D graphics, play audio, respond to mouse and keyboard events, run on
  multiple threads, and access memory directly---all without requiring
  the user to install a plug-in.
* **Portability:** Write your apps once and you'll be able to run them on
  Windows, Linux, Mac, and Chrome OS.
* **Security:** Installing a desktop app or a browser plug-in can present
  serious security risks and deter potential users. Native Client uses a
  double sandbox designed to protect resources on the user's system. This
  framework offers the safety of traditional web apps in addition to the
  performance benefits of native compiled code, without requiring users to
  install a plug-in.
* **Easy migration path to the web:** Many developers and companies have
  years of work invested in existing desktop applications. Native Client
  makes the transition from desktop app to web app significantly easier
  because Native Client supports C and C++ (and will continue to add more
  languages).
* **Performance:** Native Client allows your app to run at a speed comparable
  to a desktop app. This capability enables demanding applications such as
  console-quality games to run inside the browser.

Common use cases
================

Native Client enables you to extend web apps running in the browser with custom
features and proprietary code. Typical use cases for Native Client include the
following:

* **Existing software components:** With its native language support
  (currently C and C++), Native Client enables you to reuse current software
  modules in a web app---you don't need to spend time reinventing and debugging
  code that's already proven to work well.
* **Legacy desktop applications:** Native Client provides a smooth migration
  path from desktop application to web app. You can port and recompile
  existing code for the computation engine of your application directly to
  Native Client, and need repurpose only the user interface and event
  handling portions to the new browser platform. Native Client allows you to
  embed existing functionality directly into the browser at the same time
  your application takes advantage of things the browser does well: handling
  user interaction and processing events, based on the latest developments in
  HTML5.
* **Enterprise applications that require heavy computation:** Native Client
  handles the number crunching required by large-scale enterprise apps. To
  ensure protection of user data, Native Client enables you to build complex
  cryptographic algorithms directly into the browser so that unencrypted data
  never goes out over the network.
* **Multimedia apps:** Codecs for processing sounds, images, and movies can
  be added to the browser in a Native Client web app.
* **Games:** Native Client enables a web app to run close to native speed,
  reusing existing multithreaded/multicore C/C++ code bases, combined with
  low-level access to low-latency audio and (coming soon) networking APIs and
  OpenGL ES with programmable shaders. Programming to Native Client also
  enables your binary to run unchanged across many platforms. Native Client
  is a natural fit for running a physics engine or artificial intelligence
  module that powers a sophisticated web game.

Compiling existing native code for your app helps protect the investment you've
made in research and development. In addition to the protection offered by
Native Client compile-time restrictions, users benefit from the security
offered by its runtime validator. The validator decodes modules and limits the
instructions that can run in the browser, and the sandboxed environment proxies
system calls Users can run Native Client apps without installing a new
application or a browser plug-in, and you can update your app without requiring
user intervention.

Current limitations
===================

Native Client currently has the following limitations:

* no support for hardware exceptions
* no support for process creation / subprocesses
* no support for raw TCP/UDP sockets (analogous versions---websockets for TCP
  and peer connect for UDP---are in the works and will be available soon)
* no support for query to available memory
* inline assembly must be compatible with the Native Client validator (you
  can use the ncval utility in the SDK to check)

Some of these limitations are required to execute code safely in a browser. To
understand the reasons for these limitations read the `Google Chrome technical
booklet <http://www.google.com/googlebooks/chrome/index.html>`_ and the `Native
Client technical paper
<http://src.chromium.org/viewvc/native_client/data/docs_tarball/nacl/googleclient/native_client/documentation/nacl_paper.pdf>`_
(PDF).

How Native Client works
=======================

In order to be safe and portable for the web, the executable native code that
is produced must be sandbox-safe. More specifically, it cannot cause any
malicious or harmful effects on an end-user's machine. Native Client achieves
this security through a number of techniques, including API restrictions and
use of a modified compilation process.

Even though it provides software-based fault isolation on the x86 architecture,
Native Client still maintains the speed of native code---or thereabout. As of
current timings, the Native Client sandbox adds only about 5% overhead, which
makes it possible to do serious rendering and serious number-crunching in the
browser without a big performance hit.

A compiled Native Client module (.nexe file) is loaded into a web page through
an <embed> element. Once uploaded to a server, the .html page and the .nexe
file(s) define a web application.

To the Native Client runtime system, a Native Client module is simply a set of
machine code, formatted to adhere to a few special rules. No matter whether the
code starts out as C or C++ or any other language, the Native Client runtime
system performs the steps shown in the following figure:

.. image:: /images/NaClExecution.png

To ensure that system resources are untouched, the Native Client runtime system
prevents the following unsafe activities:

* Manipulating devices or files directly (instead, a special file system API
  is provided)
* Directly accessing the operating system
* Using self-modifying code to hide the code's intent (such as attempts to
  write to protected memory)

Salt & Pepper
-------------

Native Client product names follow a salt & pepper theme. Native Client,
abbreviated to **NaCl**, started the salt & pepper naming theme.

The Pepper Plug-in API (PPAPI), called **Pepper** for convenience, is an
open-source, cross-platform API for web browser plug-ins. From the point of
view of Native Client, Pepper is a set of APIs that allow a C or C++ Native
Client module to communicate with the hosting browser and get access to
system-level functions in a safe and portable way. One of the security
constraints in Native Client is that modules cannot make any OS-level calls.
Pepper provides analogous APIs that modules can target instead. You can use the
Pepper APIs to:

* `talk to the JavaScript code in your application
  <https://developers.google.com/native-client/devguide/coding/message-system>`_
  from the C++ code in your NaCl module
* `do FileIO
  <https://developers.google.com/native-client/devguide/coding/FileIO>`_
* `play audio
  <https://developers.google.com/native-client/devguide/coding/audio>`_
* `render 3D graphics
  <https://developers.google.com/native-client/devguide/coding/3D-graphics>`_

The figure below illustrates how Pepper serves as a bridge between Native
Client modules and the browser:

.. image:: /images/pepper.jpg

Pepper includes both a C API and a C++ API. The C++ API is a set of bindings
written on top of the C API. You can access reference documentation for both
the C API and the C++ API through the left-nav on this site. For additional
information about Pepper, see `Pepper Concepts
<http://code.google.com/p/ppapi/wiki/Concepts>`_.

Application Structure
=====================

A Native Client application is divided into three main parts:

* **HTML/JavaScript application:** Provides the user interface and event
  handling mechanisms as well as the main HTML document; can perform
  computation as well.
* **Pepper API:** Enables the JavaScript code and the Native Client module to
  send messages to each other, and provides interfaces that allow Native
  Client modules to create and use browser resources.
* **Native Client module:** Typically, performs numerical computation and
  other compute-intensive tasks. Handles large-scale data manipulation. Also
  provides event handling APIs for apps such as games where the user interface
  is integrated into the code that is run natively.

The following diagram shows how Pepper provides the bridge between the Native
Client module's code and the browser's JavaScript:

.. image:: /images/ProgramStructure.png
   :align: center
   :alt: Program Structure

Files in a Native Client application
------------------------------------

A Native Client application consists of a set of files:

* The **HTML web page**, **CSS**, and **JavaScript** files. Most Native Client
  apps contain at least an HTML document, optional CSS for styling of the web
  page, and one or more supporting JavaScript files for user interface
  objects, event handling, and simple programming tasks and calculations
  suitable for the JavaScript layer.
* The **Native Client module**. This module contains the native compiled
  code, and uses the Pepper Library (included in the SDK), which provides the
  bridge between the Native Client module and JavaScript and browser
  resources. Currently, the SDK supports the C and C++ languages for Native
  Client modules. When compiled, the extension for this filename is
  *.nexe*.
* A **manifest** file that specifies modules to load for different
  processors. This file includes a set of key-value pairs, where a key is the
  end-user's processor (for example, x86-32, x86-64, or ARM) and the
  corresponding value is the URL of the module compiled for that processor.
  The extension for this filename is *.nmf*.

HTML file
^^^^^^^^^

The HTML file contains the ``<embed>`` tag that loads the Native Client module.
For example::

   <embed name="nacl_module"
          id="hello_world"
          width=0 height=0
          src="hello_world.nmf"
          type="application/x-nacl">

The mimetype for a Native Client module, specified in the ``type`` attribute, is
``application/x-nacl``.

Native Client modules are operating system independent, but they are not (yet)
processor independent. Therefore, you must compile separate versions of a Native
Client module for x86 32-bit, x86 64-bit, ARM, and other processors. The
manifest file, specified in the ``src`` attribute of the ``<embed>``
tag, specifies which version of the Native Client module to load depending on
the end-user's processor.

Native Client module
^^^^^^^^^^^^^^^^^^^^

You can write a Native Client module in C or C++, and use existing libraries
and modules compatible with that language. After you've implemented your
functions in the module, you can use the Pepper API to pass messages to and
receive messages from the JavaScript/browser side of the application.


Creating a Native Client application
====================================

Native Client is extremely flexible, allowing for numerous ways to develop an
application. The following is one of the more common approaches:

1. :doc:`Download <sdk/download>` the Native Client SDK. You may also need to
   download and install Python, which is required to run a number of tools in
   the SDK.
2. Write the application:

   a. Create a user interface using HTML, JavaScript, and CSS.
   b. If necessary, port existing libraries to compile with your Native Client
      module. If you find yourself missing a common library, have a look at
      `NaCl ports <http://code.google.com/p/naclports>`_, a repository for open
      source libraries. Contributions are welcome.
   c. Create the Native Client module and `build
      <https://developers.google.com/native-client/devguide/devcycle/building>`_
      it using one of the NaCl toolchains in the SDK.
3. `Run
   <https://developers.google.com/native-client/devguide/devcycle/running>`_ the
   application.
4. `Distribute
   <https://developers.google.com/native-client/devguide/distributing>`_ the
   application.

For detailed, step-by-step instructions on how to create a Native Client
application, see the `Getting Started Tutorial
<https://developers.google.com/native-client/devguide/tutorial>`_. The tutorial
includes a basic set of template files that you can modify for your project.

Using existing code
===================

The Native Client SDK comes with a modified version of the GNU toolchain
(including GCC and G++) that produces sandbox-safe native code to be run in the
browser. This opens the browser to 3D games, video editing, and other
applications that can be moved to the web with relatively little effort. Some
work is required, however, to keep code safe for execution on the web. For
instance, OS-level calls must be redirected to target the Pepper API, and there
are also restrictions on how file IO and threading operations work.

Distributing a Native Client application
========================================

The Chrome browser only runs Native Client applications published through the
`Chrome Web Store
<https://chrome.google.com/webstore/search/%22native%20client%22%20OR%20nativeclient%20OR%20nacl>`_
(CWS). To distribute an application, you must compile the application for both
the 32-bit and 64-bit x86 architectures, and publish the application in the
CWS.

Native Client has not been standardized yet, and currently it requires
compilation for specific instruction set architectures. Google only allows
Native Client applications into the CWS if the applications are available for
both 32-bit and 64-bit x86 architectures; developers are also strongly
encouraged to provide a build for the ARM architecture. The intent behind the
current policy of requiring applications to be compiled for multiple
architectures is not to bake one instruction set into the web, and to make sure
that future architectures are supported as well.

Note that this is only a temporary requirement: The ultimate plan is to create
a new version of Native Client executables that can run on any processor. This
new Native Client technology is called Portable Native Client (PNaCl,
pronounced "pinnacle"), and it is already under development. Instead of
generating x86 code or ARM code, PNaCl transforms high-level code into bitcode
using a compiler based on the open source LLVM (low-level virtual machine)
project. When the browser downloads the bitcode, PNaCl then translates it to
native machine code and validates it in the same way Native Client validates
machine code today.

PNaCl could potentially give Native Client the same reach as JavaScript, but
there are still hurdles to overcome. Part of the problem is that PNaCl has more
overhead. Currently PNaCl can execute much faster than JavaScript, but it does
not yet start up as quickly. Before officially launching PNaCl, Google wants to
make sure to close that gap.

The Native Client SDK
=====================

The Native Client SDK includes the following:

* GNU-based toolchains: gcc, g++, as, ld, gdb, and other tools customized for
  Native Client
* API libraries (Pepper, POSIX)
* Makefiles for building Native Client applications
* Examples
* Documentation

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
