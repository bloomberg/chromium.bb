.. _nacl-and-pnacl:

##############
NaCl and PNaCl
##############

Introduction and historical background
======================================

Since its initial launch in 2011, Native Client enables executing native code
securely inside a web application through the use of advanced Software Fault
Isolation (SFI) techniques. TODO: link to paper?. Native Client provides
developers with the ability to harness the client machine's computational power
to a much fuller extent than traditional web technologies, by running compiled C
and C++ code at near-native speeds and taking advantage of multiple cores with
shared memory.

While Native Client provides OS independence, it still requires developers to
generate architecture-specific executable modules (**nexe**) for each hardware
platform. In addition to being inconvenient for the developer,
architecture-specific machine code is non-portable and not well suited to the
open web. The traditional method of application distribution on the web is a
self-contained bundle of HTML, CSS, JavaScript and resources (images, etc.) that
can be hosted on a server and run inside a web browser. This means that a
website created today should still work, on all platforms, years later.
Architecture-specific executables are clearly not a good fit for this
requirement.

Therefore, today Native Client can only be used in applications and browser
extensions that are installed through the Chrome Web Store. Exposing Native
Client to the open web was deemed unacceptable as long as the technology is not
fully portable.

PNaCl
=====

PNaCl addresses the portability concern by splitting the compilation process
into two parts: compiling the source code to a portable bitcode format, and
translating this format to a host-specific executable. PNaCl enables developers
to distribute *portable executables* (**pexe**-s) that the hosting environment
(e.g. the Chrome browser) can translate to native code before executing. This
aligns Native Client with existing open web technologies like JavaScript. The
developer can simply distribute a **pexe** as part of an application along with
HTML, CSS and JavaScript, and the user's machine will be able to run it. In
other words, PNaCl combines the portability of existing web technologies with
the performance and security benefits of Native Client.

With PNaCl, the developer generates a single **pexe** from his source code,
instead of a platform-specific **nexe** per architecture. The **pexe** provides
both architecture- and OS-independence. Since the **pexe** contains an abstract,
architecture-independent format, it does not have the portability problem
described in the previous section. Future versions of the hosting environment
should have no problem executing the **pexe**, even on new architectures.
Moreover, if an existing architecture is enhanced in the future, the developer's
code doesn't even have to be recompiled---in some cases the client-side
translation will be able to take advantage of the new capabilities
automatically.

With the advent of PNaCl, the distribution limitations of Native Client can be
lifted. Specifically, a **pexe** can simply be part of a web application---it
does not have to be distributed through the Chrome Web Store.

PNaCl is a new technology, so it still has a number of minor limitations when
compared to existing, non-portable Native Client; the next sections describe the
differences in detail.

When to use PNaCl?
==================

PNaCl is the preferred toolchain for Native Client and the only way to deploy
Native Client modules on the open web. Unless your project is considerably
limited by one of the factors described in the next section, use PNaCl.

Beginning with version 31, the Chrome web browser supports translation of
**pexe** modules and their usage in web applications, without requiring the user
to install the application explicitly or install any browser plugins. Native
Client and PNaCl are open-source technologies, and our hope is that they will be
added to other hosting platforms in the future.

When to use non-portable NaCl
=============================

If controlled distribution through the web store is an important part of your
product plan, the benefits of PNaCl are less important. However, note that it's
still possible to use the PNaCl toolchain for distributing applications through
the web store, and enjoy conveniences such as not needing to explicitly compile
the application for all supported architectures.

In addition, the following limitations apply to the initial release of PNaCl. If
any of these are critical for your application, use non-portable Native Client:

* By its nature, PNaCl does not support architecture-specific instructions in
  the application (*inline assembly*). Future editions of PNaCl will attempt to
  mitigate this problem by introducing portable intrinsics for vector
  operations.
* PNaCl only supports static linking with the ``newlib`` C standard library at
  this time (a PNaCl port is provided by the NaCl SDK). Dynamic linking and
  ``glibc`` are not supported. Work is under way to enable dynamic linking in
  future versions of PNaCl.
* In the initial release, PNaCl does not support C++ exception handling. 
