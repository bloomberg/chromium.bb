# base/containers library

This directory contains some STL-like containers.

Things should be moved here that are generally applicable across the code base.
Don't add things here just because you need them in one place and think others
may someday want something similar. You can put specialized containers in
your component's directory and we can promote them here later if we feel there
is broad applicability.

## Design and naming

Containers should adhere as closely to STL as possible. Functions and behaviors
not present in STL should only be added when they are related to the specific
data structure implemented by the container.

For STL-like containers our policy is that they should use STL-like naming even
when it may conflict with the style guide. So functions and class names should
be lower case with underscores. Non-STL-like classes and functions should use
Google naming. Be sure to use the base namespace.
