# Security Labels And Components

[TOC]

Bug database labels are used very heavily for security bugs. We rely on the
labels being correct for a variety of reasons, including driving fixing efforts,
driving release management efforts (merges and release notes) and also
historical queries and data mining.

Because of the extent to which we rely on labels, it is an important part of the
Security Sheriff duty to ensure that all security bugs are correctly tagged and
managed. But even if you are not the Sheriff, please fix any labeling errors you
happen upon.

Any issue that relates to security should have one of the following:

* **Security** component: Features that are related to security.
* **Type-Bug-Security**: Designates a security vulnerability that impacts users.
This label should not be used for new features that relate to security, or
general remediation/refactoring ideas. (Use the **Security** component for
that.)

## Labels Relevant For Any **Type-Bug-Security**

* **Security_Severity-**{**Critical**, **High**, **Medium**, **Low**,
**None**}: Designates the severity of a vulnerability according to our
[severity guidelines](severity-guidelines.md).
* **Pri-#**: Priority should generally match Severity:
  * **Security_Severity-Critical**: **Pri-0**.
  * **High** and **Medium**: **Pri-1**.
  * **Low**: **Pri-2**.
* **Security_Impact-**{**Head**, **Beta**, **Stable**, **None**}: Designates
which branch(es) were impacted by the bug. Only apply the label corresponding
with the earliest affected branch. **None** means that a security bug is in a
disabled feature, or otherwise doesn't impact Chrome.
* **Restrict-View-SecurityTeam** or **Restrict-View-SecurityNotify**: Labels
that restrict access to the bug for members of security@chromium.org or
security-notify@chromium.org, respectively. Should a bug ever contain
confidential information, or if the reporter wishes to remain anonymous, we add
**Restrict-View-Google** and/or **Restrict-View-SecurityEmbargo**.
* **reward-**{**topanel**, **unpaid**, **na**, **inprocess**, _#_}: Labels used
in tracking bugs nominated for our [Vulnerability Reward
Program](https://www.chromium.org/Home/chromium-security/vulnerability-rewards-program).
* **M-#**: Target milestone for the fix.
* Component: For bugs filed as **Type-Bug-Security**, we also want to track
which component(s) the bug is in.
* **ReleaseBlock-Stable**: When we find a security bug regression that has not
yet shipped to stable, we use this label to try and prevent the security
regression from ever affecting users of the Stable channel.
* **OS-**{**Chrome**, **Linux**, **Windows**, ...}: Denotes which operating
systems are affected.
* **Merge-**{**Request-?**, **Approved-?**, **Merged-?**}: Security fixes
are frequently merged to earlier release branches.
* **Release-#-M##**: Denotes which exact patch a security fix made it into.
This is more fine-grained than the **M-** label. **Release-0-M50** denotes the
initial release of a M50 to Stable.
* **CVE-####-####**: For security bugs that get assigned a CVE, we tag the
appropriate bug(s) with the label for easy searching.

## An Example

Given the importance and volume of labels, an example might be useful.

1. An external researcher files a security bug, with a repro that demonstrates
memory corruption against the latest (e.g.) M29 dev channel. The labels
**Restrict-View-SecurityTeam** and **Type-Bug-Security** will be applied.
1. The sheriff jumps right on it and uses ClusterFuzz to confirm that the bug is
a novel and nasty-looking buffer overflow in the renderer process. ClusterFuzz
also confirms that all current releases are affected. Since M27 is the current
Stable release, and M28 is in Beta, we add the labels of the earliest affected
release: **M-27**, **Security_Impact-Stable**. The severity of a buffer overflow
in a renderer implies **Security_Severity-High** and **Pri-1**. Any external
report for a confirmed vulnerability needs **reward-topanel**. Sheriffbot will
usually add it automatically. The stack trace provided by ClusterFuzz suggests
that the bug is in the component **Blink>DOM**, and such bugs should be labeled
as applying to all OSs except iOS (where Blink is not used): **OS-**{**Linux**,
**Windows**, **Android**, **Chrome**, **Fuchsia**}.
1. Within a day or two, the sheriff was able to get the bug assigned and — oh
joy! — fixed very quickly. When the bug's status changes to **Fixed**,
Sheriffbot will add the **Merge-Requested** label, and will change
**Restrict-View-SecurityTeam** to **Restrict-View-SecurityNotify**.
1. Later that week, the Chrome Security manager does a sweep of all
**reward-topanel** bugs. This one gets rewarded, so that one reward label is
replaced with two: **reward-1000** and **reward-unpaid**. Later,
**reward-unpaid** becomes **reward-inprocess** and is later still removed when
all is done. Of course, **reward-1000** remains forever. (We use it to track
total payout!)
1. The next week, a Chrome TPM states that the first Chrome M27 stable patch is
going on and asks if we want to include security fixes. We do, of course.
Having had this particular fix "bake" in another M29 Dev channel, the Chrome
Security release manager decides to merge it. **Merge-Approved-M##** is
replaced with **Merge-Merged-M##**. (IMPORTANT: This transition only occurs
after the fix is merged to ALL applicable branches, i.e. M28 as well as M27 in
this case.) We now know that users of Chrome Stable will get the fix in the
first M27 Stable patch, so the following labels are changed/applied: **M-27**,
**Release-1**. Since the bug was externally reported, it definitely gets its
own CVE, and a label is eventually added: **CVE-2013-31337**.
1. 14 weeks after the bug is marked **Fixed**, Sheriffbot removes the
**Restrict-View-SecurityNotify** label and other **Restrict-View-?** labels,
making the bug public. There is one crucial exception: Sheriffbot will not
remove **Restrict-View-SecurityEmbargo**.
