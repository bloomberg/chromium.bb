This directory exists to allow iOS code that is not yet upstreamed to call
Chromium code without being vulnerable to breakage during a merge.
Specifically, not-yet-upstreamed code is allowed to use the interfaces
provided in public/. Any change to one of these interfaces should get a full
review from an OWNER, as such a change will require corresponding changes to
code not yet upstreamed. Any change to code not under public/ can be TBR'd to
an OWNER.
