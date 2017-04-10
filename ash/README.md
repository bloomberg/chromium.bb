Ash
---
Ash is the "Aura Shell", the window manager and system UI for Chrome OS.
Ash uses the views UI toolkit (e.g. views::View, views::Widget, etc.) backed
by the aura native widget and layer implementations.

Ash sits below chrome in the dependency graph (i.e. it cannot depend on code
in //chrome). It has a few dependencies on //content, but these are isolated
in their own module in //ash/content. This allows targets like ash_unittests
to build more quickly.

Tests
-----
Most tests should be added to the ash_unittests target. Tests that rely on
//content should be added to ash_content_unittests, but these should be rare.

Tests can bring up most of the ash UI and simulate a login session by deriving
from AshTestBase. This is often needed to test code that depends on ash::Shell
and the controllers it owns.

Mus+ash
----------
Ash is transitioning to use the mus window server and gpu process, found in
//services/ui. Ash continues to use aura, but aura is backed by mus. Code to
support mus is found in //ash/mus. There should be relatively few differences
between the pure aura and the aura-mus versions of ash. Ash can by run in mus
mode by passing the --mus command line flag.

Ash is also transitioning to run as a mojo service in its own process. This
means that code in chrome cannot call into ash directly, but must use the mojo
interfaces in //ash/public/interfaces.

Out-of-process Ash is referred to as "mash" (mojo ash). In-process ash is
referred to as "classic ash". Ash can run in either mode depending on the
--mash command line flag.

Historical notes
----------------
Ash shipped on Windows for a couple years to support Windows 8 Metro mode.
Windows support was removed in 2016.
