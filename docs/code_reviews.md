# Code Reviews

Code reviews are a central part of developing high-quality code for Chromium.
All changes must be reviewed.

The bigger patch-upload-and-land process is covered in more detail the
[contributing code](https://www.chromium.org/developers/contributing-code)
page.

# Code review policies

Ideally the reviewer is someone who is familiar with the area of code you are
touching. If you have doubts, look at the git blame for the file and the OWNERS
files (see below).

Any committer can review code, but there must be at least one owner for each
directory you are touching. If you have multiple reviewers, make it clear in
the message you send requesting review what you expect from each reviewer.
Otherwise people might assume their input is not required or waste time with
redundant reviews.

## Expectations for all reviewers

  * Aim to provide some kind of actionable response within 24 hours of receipt
    (not counting weekends and holidays). This doesn't mean you have to have
    done a complete review, but you should be able to give some initial
    feedback, request more time, or suggest another reviewer.

  * It can be nice to indicate if you're away in your name in the code review
    tool. If you do this, indicate when you'll be back.

  * Don't generally discourage people from sending you code reviews. This
    includes writing a blanket ("slow") after your name in the review tool.

## OWNERS files

In various directories there are files named "OWNERS" that list the email
addresses of people qualified to review changes in that directory. You must
get a positive review from an owner of each directory your change touches.

Owners files are recursive, so each file also applies to its subdirectories. If
an owner file contains the text "set noparent" it will stop owner propagation
from parent directories.

Tip: The "git cl owners" command can help find owners.

While owners must approve all patches, any committer can contribute to the
review. In some directories the owners can be overloaded or there might be
people not listed as owners who are more familiar with the low-level code in
question. In these cases it's common to request a low-level review from an
appropriate person, and then request a high-level owner review once that's
complete. As always, be clear what you expect of each reviewer to avoid
duplicated work.

### Expectations for owners

  * OWNERS files should contain only people actively reviewing code in that
    directory.

  * If you aren't doing code reviews in a directory, remove yourself. Don't
    try to discourage people from sending reviews in comments in the OWNERS
    file. This includes writing "slow" or "emeritus" after your name.

  * If the code review load is unsustainable, work to expand the number of
    owners.

## TBR ("To Be Reviewed")

"TBR" is our mechanism for post-commit review. It should be used rarely and
only in cases where a review is unnecessary or as described below. The most
common use of TBR is to revert patches that broke the build.

TBR does not mean "no review." A reviewer on a TBRed on a change should still
review the change. If there comments after landing the author is still
obligated to address them in a followup patch.

Do not use TBR just because a change is urgent or the reviewer is being slow.
Work to contact a reviewer who can do your review in a timely manner.

To send a change TBR, annotate the description and send email like normal.
Otherwise the reviewer won't know to review the patch.

  * Add the reviewer's email address in the code review tool's reviewer field
    like normal.

  * Add a line "TBR=<reviewer's email>" to the bottom of the change list
    description.

  * Push the "send mail" button.

### TBR-ing certain types of mechanical changes

Sometimes you might do something that affects many callers in different
directories. For example, adding a parameter to a common function in //base.
If the updates to the callers is mechanical, you can:

  * Get a normal owner of the lower-level directory (in this example, //base)
    you're changing to do a proper review of those changes.

  * Get _somebody_ to review the downstream changes. This is often the same
    person from the previous step but could be somebody else.

  * Add the owners of the affected downstream directories as TBR.

This process ensures that all code is reviewed prior to checkin and that the
concept of the change is reviewed by a qualified person, but you don't have to
track down many individual owners for trivial changes to their directories.

### TBR-ing documentation updates

You can TBR documentation updates. Documentation means markdown files, text
documents, and high-level comments in code. At finer levels of detail, comments
in source files become more like code and should be reviewed normally (not
using TBR). Non-TBR-able stuff includes things like function contracts and most
comments inside functions.

  * Use your best judgement. If you're changing something very important,
    tricky, or something you may not be very familiar with, ask for the code
    review up-front.

  * Don't TBR changes to policy documents like the style guide or this document.

  * Don't mix unrelated documentation updates with code changes.

  * Be sure to actually send out the email for the code review. If you get one,
    please actually read the changes. Even the best writers have editors and
    checking prose for clarity is much faster than checking code for
    correctness.
