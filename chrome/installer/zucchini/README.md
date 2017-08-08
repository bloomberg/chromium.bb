
Basic Definitions for Patching
------------------------------

**Binary**: Executable image and data. Binaries may persist in an archive
(e.g., chrome.7z), and need to be periodically updated. Formats for binaries
include {PE files EXE / DLL, ELF, DEX}. Architectures binaries include
{x86, x64, ARM, AArch64, Dalvik}. A binary is also referred to as an executable
or an image file.

**Patching**: Sending a "new" file to clients who have an "old" file by
computing and transmitting a "patch" that can be used to transform "old" into
"new".  Patches are compressed for transmission. A key performance metric is
patch size, which refers to the size of compressed patch file. For our
experiments we use 7z.

**Patch generation**: Computation of a "patch" from "old" and "new". This can be
expensive (e.g., ~15-20 min for Chrome, using 1 GB of RAM), but since patch
generation is a run-once step on the server-side when releasing "new" binaries,
the expense is not too critical.

**Patch application**: Transformation from "old" binaries to "new", using a
(downloaded) "patch".  This is executed on client side on updates, so resource
constraints (e.g., time, RAM, disk space) is more stringent. Also, fault-
tolerance is important. This is usually achieved by an update system by having
a fallback method of directly downloading "new" in case of patching failure.

**Offset**: Position relative to the start of a file.

**Local offset**: An offset relative to the start of a region of a file.

**Reference**: A directed connection between two offsets in a binary. For
example, consider jump instructions in x86:

    00401000: E9 3D 00 00 00     jmp         00401042

Here, the 4 bytes `[3D 00 00 00]` starting at address `00401001` point to
address `00401042` in memory. This forms a reference from `offset(00401001)`
(length `4`) to `offset(00401042)`, where `offset(addr)` indicates the disk
offset corresponding to `addr`.

**Location**: The starting offset of bytes that store a reference. In the
preceding example, `offset(00401001)` is a location. Each location is associated
with a span of bytes that encodes reference data.

**Target**: The offset that's the destination of a reference. In the preceding
example, `offset(00401042)` is a target. Different references can share common
targets. For example, in

    00401000: E9 3D 00 00 00     jmp         00401042
    00401005: EB 3B              jmp         00401042

we have two references with different locations, but same target.

Because the bytes that encode a reference depend on its target, and potentially
on its location, they are more likely to get modified from an old version of a
binary to a newer version. This is why "naive" patching does not do well on
binaries.

**Disassembler**: Architecture specific data and operations, used to extract and
correct references in a binary.

**Type of reference**: The type of a reference determines the binary
representation used to encode its target. This affects how references are parsed
and written by a disassembler. There can be many types of references in the same
binary.

A reference is represented by the tuple (disassembler, location, target, type).
This tuple contains sufficient information to write the reference in a binary.

**Pool of targets**: Collection of targets that is assumed to have some semantic
relationship. Each reference type belong to exactly one reference pool. Targets
for references in the same pool are shared.

For example, the following describes two pools defined for Dalvik Executable
format (DEX). Both pools spawn mutiple types of references.

1. Index in string table.
  - From bytecode to string index using 16 bits.
  - From bytecode to string index using 32 bits.
  - From field item to string index using 32 bits.
2. Address in code.
  - Relative 16 bits pointer.
  - Relative 32 bits pointer.

Boundaries between different pools can be ambiguous. Having all targets belong
to the same pool can reduce redundancy, but will use more memory and might
cause larger corrections to happen, so this is a tradeoff that can be resolved
with benchmarks.

**Abs32 references**: References whose targets are adjusted by the OS during
program load. In an image, a **relocation table** typically provides locations
of abs32 references. At each abs32 location, the stored bytes then encode
semantic information about the target (e.g., as RVA).

**Rel32 references**: References embedded within machine code, in which targets
are encoded as some delta relative to the reference's location. Typical examples
of rel32 references are branching instructions and instruction pointer-relative
memory access.

**Equivalence**: A (src_offset, dst_offset, length) tuple describing a region of
"old" binary, at an offset of |src_offset|, that is similar to a region of "new"
binary, at an offset of |dst_offset|.

**Associated Targets**: A target in "old" binary is associated with a target in
"new" binary if both targets:
1. are part of similar regions from the same equivalence, and
2. have the same local offset (relative to respective start regions), and
3. are not part of any larger region from a different equivalence.
Not all targets are necessarily associated with another target.

**Label**: An (offset, index) pair, where |offset| is a target, and |index| is
an integer used to uniquely identify |offset| in its corresponding pool of
targets. Labels are created for each Reference in "old" and "new" binary as part
of generating a patch, and used to alias targets when searching for similar
regions that will form equivalences. Labels are created such that associated
targets in old and new binaries share the same |index|, and such that indices in
a pool are tightly packed. For example, suppose "old" Labels are:
  - (0x1111, 0), (0x3333, 4), (0x5555, 1), (0x7777, 3)
and given the following association of targets between "old" and "new":
  - 0x1111 <=> 0x6666,  0x3333 <=> 0x2222.
then we could assign indices for "new" Labels as:
  - (0x2222, 4}, (0x4444, 8), (0x6666, 0), (0x8888, 2)

**Encoded Image**: The result of projecting the content of an image to scalar
values that describe content on a higher level of abstraction, masking away
undesirable noise in raw content. Notably, the projection encodes references
based on their associated label.
