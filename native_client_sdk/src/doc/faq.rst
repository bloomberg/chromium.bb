.. _faq:

##########################
Frequently Asked Questions
##########################


.. contents::
  :local:
  :backlinks: none
  :depth: 2

This document answers some frequently asked questions about Native
Client (NaCl) and Portable Native Client (PNaCl, pronounced
"pinnacle"). For a high-level overview of Native Client, see the
:doc:`Technical Overview <overview>`.

If you have questions that aren't covered in this FAQ:

* Scan through the :doc:`Release Notes <sdk/release-notes>`.
* Search through or ask on the :doc:`Native Client Forums <help>`.

What is Native Client Good For?
===============================

Why did Google build Native Client?
-----------------------------------

* **Performance:** Native Client modules run nearly as fast as native
  compiled code.
* **Security:** Native Client lets users run native compiled code in the
  browser with the same level of security as traditional web apps. Users
  get the performance boost of running compiled code without having to
  install a plugin.
* **Convenience:** Developers can leverage existing C and C++ code in
  their web apps.
* **Portability:** Native Client applications run on Windows, Mac, and
  Linux. When development of `Portable Native Client
  <http://nativeclient.googlecode.com/svn/data/site/pnacl.pdf>`__.
  (PDF) is complete, Native Client applications will also be
  processor-independent.

When should I use Native Client?
--------------------------------

The following are some typical use cases. For details, see the
:doc:`Technical Overview <overview>`.

* Porting existing software components for use in a web app.
* Porting legacy desktop applications.
* Handling browser-side encryption and decryption for an enterprise
  application.
* Handling multimedia for a web app.
* Handling various aspects of web-based games, including physics engines
  and AI.

Native Client is a versatile technology; we expect that it will also be
used in many other contexts.

How fast does code run in Native Client?
----------------------------------------

Fast! Initial benchmarks on x86-32 measured an average performance
overhead of less than 5% compared to native C/C++ on applications such
as Quake, bzip2, and Google Earth. For details of those benchmarks, see
`Native Client: A Sandbox for Portable, Untrusted x86 Code
<http://src.chromium.org/viewvc/native_client/data/docs_tarball/nacl/googleclient/native_client/documentation/nacl_paper.pdf>`_
(PDF).

Benchmarks on x86-64 and ARM measured an average performance overhead of
less than 5% on ARM and 7% on x86-64; however, benchmark performance was
bimodal for x86-64, so different use cases are likely to achieve either
significantly better or significantly worse performance than that
average. For details, see `Adapting Software Fault Isolation to
Contemporary CPU Architectures
<http://nativeclient.googlecode.com/svn/data/site/NaCl_SFI.pdf>`_ (PDF).

Why use Native Client instead of Flash, Java, Silverlight, ActiveX, .NET, or other such technologies?
-----------------------------------------------------------------------------------------------------

Different technologies have different strengths and weaknesses. In
appropriate contexts, Native Client can be faster, more secure, and/or
more compatible across operating systems than some other technologies.

Native Client complements other technologies by giving web developers a
new capability: the ability to run fast, native code from a web browser.

If I want direct access to the OS, should I use Native Client?
--------------------------------------------------------------

No --- Native Client does not provide direct access to the OS or
devices, or otherwise bypass the JavaScript security model. For more
information, see later sections of this FAQ.

Development Environments and Tools
==================================

Can I develop on Windows/Mac/Linux? What development environment do you recommend?
----------------------------------------------------------------------------------

Yes, you can develop on Windows, Mac, or Linux, and the resulting Native
Client application will run inside the browser for Google Chrome users
on all those platforms as well as ChromeOS.

On all three platforms, you can use Eclipse for C/C++ developers --- or
you can simply use your own favorite editor and compile on the command
line. If you're developing in Mac OS X, you can use Xcode. For Windows,
we will be supporting Microsoft Visual Studio in a future release.

I'm not familiar with tools like GCC, make, Eclipse, Visual Studio, or Xcode. Can I still use the Native Client SDK?
--------------------------------------------------------------------------------------------------------------------

You'll need to learn how to use some of those tools before you can get
very far with the SDK. Try seaching for an `introduction to GCC
<htp://www.google.com/search?q=gcc+introduction>`_.

You may also find our :doc:`Tutorial <devguide/tutorial>` and
:doc:`Building instructions <devguide/devcycle/building>` useful, and
you can look at the code and Makefiles for the SDK examples to
understand how the examples are built and run.

Openness, and Supported Architectures and Languages
===================================================


Is Native Client open? Is it a standard?
----------------------------------------

Native Client is completely open: the executable format is open and the
source code is open. Right now the Native Client project is in its early
stages, so it's premature to consider Native Client for standardization.

What are the supported instruction set architectures? Will Native Client modules run only on Intel/AMD x86 processors? If so, why are you putting architecture-dependent technology on the web?
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

Native Client modules currently run on the x86-32, x86-64, and ARM
architectures, but an important goal of Native Client is to be
platform-independent. We are developing a technology called `Portable
Native Client
<http://nativeclient.googlecode.com/svn/data/site/pnacl.pdf>`__ (PDF)
that will provide an instruction-set-neutral format. With Portable
Native Client we believe we can deliver a system that has comparable
portability to JavaScript. The web is better when it's
platform-independent, and we'd like it to stay that way.

Do I have to use C or C++? I'd really like to use another language.
-------------------------------------------------------------------

Right now only C and C++ are supported directly by the toolchain in the
SDK. C# and other languages in the .NET family are supported via the
`Mono port <https://github.com/elijahtaylor/mono>`_ for Native
Client. Moreover, there are several ongoing projects to support
additional language runtimes as well as to compile more languages to
LLVM, C, or C++.

If you're interested in getting other languages working, please contact
the Native Client team by way of the `native-client-discuss mailing list
<http://groups.google.com/group/native-client-discuss>`_.

Will you only support Chrome? What about other browsers?
--------------------------------------------------------

We aim to support multiple browsers. However, a number of features that
we consider requirements for a production-quality system are difficult
to implement without help from the browser. Specific examples are an
out-of-process plugin architecture and appropriate interfaces for
integrated 3D graphics. We have worked closely with Chromium developers
to deliver these features and we would be eager to collaborate with
developers from other browsers.

What's the difference between NPAPI and Pepper?
-----------------------------------------------

:doc:`Pepper <peppercpp/index>` (also known as PPAPI) is a new API that
lets Native Client modules communicate with the browser. Pepper supports
various features that don't have robust support in NPAPI, such as event
handling, out-of-process plugins, and asynchronous interfaces. Native
Client has transitioned from using NPAPI to using Pepper.

Is NPAPI part of the Native Client SDK?
---------------------------------------

NPAPI is not supported by the Native Client SDK, and is `deprecated in
Chrome
<http://blog.chromium.org/2013/09/saying-goodbye-to-our-old-friend-npapi.html>`_.

Does Native Client support SSE and NEON?
----------------------------------------

Yes.

Coming Soon
===========


Do Native Client modules have access to serial ports, camera devices, or microphones?
-------------------------------------------------------------------------------------

Not at this time; Native Client can only use native resources that
today's browsers can access. However, we intend to recommend such
features to the standards bodies and piggyback on their efforts to make
these resources available inside the browser.

You can generally think of Pepper as the C/C++ bindings to the
capabilities of HTML5. The goal is for Pepper and JavaScript to evolve
together and stay on par with each other with respect to features and
capabilities.

Can I use Native Client for 3D graphics?
----------------------------------------

Yes. Native Client supports `OpenGL ES 2.0
<http://www.khronos.org/opengles/>`_.

Does Native Client support HTML5 native Web Workers?
----------------------------------------------------

No, but we do support pthreads, thus allowing your Native Client app to
utilize several CPU cores.

Security and Privacy
====================

If I use Native Client, will Google collect my data, sell my data, own my application, trace the usage, or steal my dog?
------------------------------------------------------------------------------------------------------------------------

No, none of the above. If you opt in to sending usage statistics in
Chrome, then if Native Client crashes, Chrome will send debug
information to Google. But crashes in your code won't send your
information to Google; Google counts the number of such crashes, but
does so anonymously.

For additional information about privacy and Chrome, see the `Google
Chrome privacy policy
<http://www.google.com/chrome/intl/en/privacy.html>`_ and the `Google
Chrome Terms of Service
<http://www.google.com/chrome/intl/en/eula_text.html>`_.

How does Native Client prevent sandboxed code from getting out and doing Bad Things? How can Google predict all possible code that could break out of the sandbox?
------------------------------------------------------------------------------------------------------------------------------------------------------------------

Native Client's sandbox works by validating the untrusted code (the
compiled Native Client module) before running it. The validator checks
the following:

* Data integrity. No loads or stores are permitted outside of the data
  sandbox. In particular this means that once loaded into memory, the
  binary is not writable. This is enforced by operating system
  protection mechanisms. While new instructions can be inserted at
  runtime to support things like JIT compilers, such instructions will
  be subject to runtime verification according to the following
  constraints before they are executed.
* No unsafe instructions. The validator ensures that the Native Client
  application does not contain any unsafe instructions. Examples of
  unsafe instructions are ``syscall``, ``int``, and ``lds``.
* Control flow integrity. The validator ensures that all direct and
  indirect branches target a safe instruction.

In addition to static analysis of untrusted code, the Native Client
runtime also includes an outer sandbox that mediates system calls. For
more details about both sandboxes, see `Native Client: A Sandbox for
Portable, Untrusted x86 Code
<http://src.chromium.org/viewvc/native_client/data/docs_tarball/nacl/googleclient/native_client/documentation/nacl_paper.pdf>`_
(PDF).

How does Google know that the safety measures in Native Client are sufficient?
------------------------------------------------------------------------------

Google has taken several steps to ensure that Native Client's security
works, including:

* Open source, peer-reviewed papers describing the design.
* A `security contest
  <https://developers.google.com/native-client/community/security-contest/>`_.
* Multiple internal and external security reviews.
* The ongoing vigilance of our engineering and developer community.

.. TODO: Fix security contest link once ReST-ified.

Google is committed to making Native Client as safe as, or safer than,
JavaScript and other popular browser technologies. If you have
suggestions for security improvements, let the team know, by way of the
`native-client-discuss mailing list
<http://groups.google.com/group/native-client-discuss>`_.

Development
===========

How do I debug?
---------------

Use of a debugger isn't yet supported in Native Client, though you may
be able to debug Native Client modules with some :doc:`alternative
approaches <devguide/devcycle/debugging>`. We're actively working on
providing integrated debugging support. For further developments, keep
an eye on the `native-client-announce mailing list
<http://groups.google.com/group/native-client-announce>`_.

How do I build x86-64 or ARM ``.nexes``?
----------------------------------------

By default, the applications in the ``/examples`` folder compile for
x86-32, x86-64, and ARM. To specifically target x86-64, compile your
module using ``<platform>_x86_<library>/bin/x86_64-nacl-g++``. To
specifically target ARM, compile your module using
``<platform>_arm_newlib/bin/arm-nacl-g++``. For more information, see
the :doc:`Building instructions <devguide/devcycle/building>`.

How can my web app determine whether to load the x86-32, x86-64, or ARM ``.nexe``?
----------------------------------------------------------------------------------

Your application does not need to make that decision explicitly --- the
Native Client runtime examines a manifest file (.nmf) to pick the right
``.nexe`` file for a given user. You can generate a manifest file using
a Python script that's included in the SDK (see the Makefile in any of
the SDK examples for an illustration of how to do so). Your HTML file
specifies the manifest filename in the ``src`` attribute of the
``<embed>`` tag. You can see the way the pieces fit together by
examining the examples included in the SDK.

.. Note::
  :class: note

  The forthcoming `Portable Native Client
  <http://blog.chromium.org/2010/03/native-client-and-web-portability.html>`__
  technology (also known as "PNaCl") will remove the requirement to
  build multiple ``.nexe`` files to support multiple architectures.

Is it possible to build a Native Client module with just plain C (not C++)?
---------------------------------------------------------------------------

Yes. See the Hello, World in C example in the SDK (in
``examples/tutorial/using_ppapi_simple/``).

What UNIX system calls can I make through Native Client?
--------------------------------------------------------

Native Client doesn't expose any of the host OS's system calls directly,
because of the inherent security risks and because the resulting app
would not be portable across operating systems. Instead, Native Client
provides varying degrees of wrapping or proxying of the host OS's
functionality or emulation of UNIX system calls. For example, Native
Client provides an ``mmap()`` system call that behaves much like the
standard UNIX ``mmap()`` system call.

Is my favorite third-party library available for Native Client?
---------------------------------------------------------------

Google has ported several third-party libraries to Native Client; such
libraries are available in the `naclports
<http://code.google.com/p/naclports>`_ project. We encourage you to
contribute libraries to naclports, and/or to host your own ported
libraries, and to `let the team know about it
<http://groups.google.com/group/native-client-discuss>`_ when you do.

Do all the files in a Native Client application need to be served from the same domain?
---------------------------------------------------------------------------------------

The ``.html``, ``.nmf``, and ``.nexe`` files must be served from the
same domain and the Chrome Web Store manifest file must include the
correct, verified domain. Other files can be served from the same or
another domain.

Portability
===========

Do I have to do anything special to make my Native Client web app run on different operating systems?
-----------------------------------------------------------------------------------------------------

No. Native Client apps run without modification on all supported
operating systems.

However, to run on different instruction set architectures (such as
x86-32 and x86-64), you currently have to build and supply a separate
``.nexe`` file for each architecture. See :doc:`target architectures
<devguide/devcycle/building>` for details about which ``.nexe`` files
will run on which architectures.

`Portable Native Client
<http://blog.chromium.org/2010/03/native-client-and-web-portability.html>`__
will remove the requirement to build multiple ``.nexe`` files.

How easy is it to port my existing native code to Native Client?
----------------------------------------------------------------

In most cases, you won't have to rewrite a lot of code. The Native
Client-specific GNU tools, such as ``x86_64-nacl-g++``, take care of
most of the necessary changes. You may need to make some changes to your
operating system calls and interactions with external devices to work in
the web world; but (for example) porting existing Linux libraries is
generally straightforward, with large libraries often requiring no
source change.

The following kinds of code may be hard to port:

* Code that does direct TCP/IP or UDP networking. For security reasons,
  Native Client is restricted to the networking otherwise available in
  the browser.
* Code that needs to do local file I/O. Native Client is restricted to
  accessing URLs and to local storage in the browser (the Pepper file
  I/O API has access to the same per-application storage that JavaScript
  has via Local Storage).
* Code that creates processes, including UNIX forks. Creating processes
  is not supported for security reasons. However, threads are supported.

.. _faq_troubleshooting:

Troubleshooting
===============

My ``.nexe`` files never finish loading. What gives?
----------------------------------------------------

Here are ways to resolve some common problems that can prevent loading:

* You must use Google Chrome version 14 or greater.
* If you haven't already done so, enable the Native Client flag in
  Google Chrome. Type ``about:flags`` in the Chrome address bar, scroll
  down to "Native Client", click the "Enable" link, scroll down to the
  bottom of the page, and click the "Relaunch Now" button (all browser
  windows will restart).
* Verify that the Native Client plugin is enabled in Google Chrome. Type
  ``about:plugins`` in the Chrome address bar, scroll down to "Native
  Client", and click the "Enable" link. (You do not need to relaunch
  Chrome after you enable the Native Client plugin).
* Make sure that the ``.nexe`` files are being served from a web
  server. Native Client uses the same-origin security policy, which
  means that modules will not load in pages opened with the ``file://``
  protocol. In particular, you can't run the examples in the SDK by
  simply dragging the HTML files from the desktop into the browser. See
  :doc:`Running Native Client Applications <devguide/devcycle/running>`
  for instructions on how to run the httpd.py mini-server included in
  the SDK.
* The ``.nexe`` files must have been compiled using SDK version 0.5 or
  greater.
* You must load the correct ``.nexe`` file for your machine's specific
  instruction set architecture (such as x86-32 or x86-64). You can
  ensure you're loading the correct ``.nexe`` file by building a
  separate ``.nexe`` for each architecture, and using a ``.nmf``
  manifest file to let the browser select the correct ``.nexe``
  file. Note: the need to select a processor-specifc ``.nexe`` will go
  away with `Portable Native Client
  <http://blog.chromium.org/2010/03/native-client-and-web-portability.html>`__.
* If things still aren't working, `drop a note to the team
  <http://groups.google.com/group/native-client-discuss>`_.
