# Use Qt Creator as IDE or GUI Debugger

[Qt Creator](https://www.qt.io/ide/)
([Wiki](https://en.wikipedia.org/wiki/Qt_Creator)) is a cross-platform C++ IDE. 

You can use Qt Creator as a daily IDE or just as a GDB frontend and that does 
not require project configuration.

[TOC]

## IDE

### Workflow

1. `ctrl+k` Activate Locator, you can open file(not support sublime-like-search) or type `. ` go to symbol.
2. `ctrl+r` Build and Run, `F5` Debug.
3. `F4` switch between header file and cpp file.
4. `ctrl+shift+r` rename symbol under cursor.
5. Code completion is built-in. And you can add your snippets.

### Setup as IDE

1. Install latest Qt Creator
2. under chromium/src `gn gen out/Default --ide=qtcreator`
3. qtcreator out/Default/qtcreator_project/all.creator

It takes 3 minutes to parsing C++ files in my workstation!!! And It will not 
block you while parsing.

#### Code Style

1. Help - About Plugins enable Beautifier.
2. Tools - Options - Beautifier - Clang Format, select use predefined style: 
chromium. You can also set a keyboard shortcut for it.
3. Tools - Options - Code Style import this xml file

```
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE QtCreatorCodeStyle>
<!-- Written by QtCreator 4.2.1, 2017-02-08T19:07:34. -->
<qtcreator>
 <data>
  <variable>CodeStyleData</variable>
  <valuemap type="QVariantMap">
   <value type="bool" key="AlignAssignments">true</value>
   <value type="bool" key="AutoSpacesForTabs">false</value>
   <value type="bool" key="BindStarToIdentifier">false</value>
   <value type="bool" key="BindStarToLeftSpecifier">false</value>
   <value type="bool" key="BindStarToRightSpecifier">false</value>
   <value type="bool" key="BindStarToTypeName">true</value>
   <value type="bool" 
     key="ExtraPaddingForConditionsIfConfusingAlign">true</value>
   <value type="bool" key="IndentAccessSpecifiers">true</value>
   <value type="bool" key="IndentBlockBody">true</value>
   <value type="bool" key="IndentBlockBraces">false</value>
   <value type="bool" key="IndentBlocksRelativeToSwitchLabels">false</value>
   <value type="bool" key="IndentClassBraces">false</value>
   <value type="bool" key="IndentControlFlowRelativeToSwitchLabels">true</value>
   <value type="bool"
     key="IndentDeclarationsRelativeToAccessSpecifiers">false</value>
   <value type="bool" key="IndentEnumBraces">false</value>
   <value type="bool" key="IndentFunctionBody">true</value>
   <value type="bool" key="IndentFunctionBraces">false</value>
   <value type="bool" key="IndentNamespaceBody">false</value>
   <value type="bool" key="IndentNamespaceBraces">false</value>
   <value type="int" key="IndentSize">2</value>
   <value type="bool" key="IndentStatementsRelativeToSwitchLabels">true</value>
   <value type="bool" key="IndentSwitchLabels">false</value>
   <value type="int" key="PaddingMode">2</value>
   <value type="bool" key="ShortGetterName">true</value>
   <value type="bool" key="SpacesForTabs">true</value>
   <value type="int" key="TabSize">2</value>
  </valuemap>
 </data>
 <data>
  <variable>DisplayName</variable>
  <value type="QString">chrome</value>
 </data>
</qtcreator>
```

#### Build & Run

In left panel, projects - setup the ninja command in build and clean step and   
executable chrome path in run.

## Debugger

**You can skip the project settings and use QtCreator as a single file 
standalone GDB frontend. **

1. Tools - Options - Build & Run - Debuggers, make sure GDB is set.
2. Tools - Options - Kits, change the Desktop kit to GDB(LLDB doesnot work in 
  Linux).
3. Open file you want to debug.
4. Debug - Start Debugging - Attach to running Application, you may need to 
  open chrome's task manager to find the process number.

### Tips, tricks, and troubleshooting

#### Make GDB start fast

Add `gdb_index = true` to `gn args`.

#### Debugger shows start then finish

```
$ echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
```

Ensure yama allow you to attach another process.

#### Debugger do not stop in break point

Ensure you are using GDB not LLDB in Linux.

#### More

See 
https://chromium.googlesource.com/chromium/src/+/master/docs/linux_debugging.md