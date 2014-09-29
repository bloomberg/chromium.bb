<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" encoding="UTF-8" indent="yes" />
<xsl:param name="which" />

<xsl:template match="/">
  <!-- insert docbook's DOCTYPE blurb -->
    <xsl:text disable-output-escaping = "yes"><![CDATA[
<!DOCTYPE appendix PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN" "http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd" [
  <!ENTITY % BOOK_ENTITIES SYSTEM "Wayland.ent">
%BOOK_ENTITIES;
]>
]]></xsl:text>

  <section id="sect-Library-$which">
    <xsl:attribute name="id">sect-Library-<xsl:value-of select="$which"/></xsl:attribute>
    <title><xsl:value-of select="$which"/> API</title>
    <para>Following is the Wayland library classes for the <xsl:value-of select="$which"/>
      (<emphasis>libwayland-<xsl:value-of select="translate($which, 'SC', 'sc')"/></emphasis>).
      Note that most of the procedures are related with IPC, which is the main responsibility of
      the library.
    </para>

    <xsl:if test="/doxygen/compounddef[@kind='class']">
      <para>
        <variablelist>
          <xsl:apply-templates select="/doxygen/compounddef" />
        </variablelist>
      </para>
    </xsl:if>

    <para>Methods for the respective classes.</para>

    <para>
    <variablelist>
    <xsl:apply-templates select="/doxygen/compounddef/sectiondef/memberdef" />
    </variablelist>
    </para>
  </section>
</xsl:template>

<xsl:template match="parameteritem">
    <varlistentry>
        <term>
          <xsl:apply-templates select="parameternamelist/parametername"/>
        </term>
      <listitem>
        <para>
          <xsl:apply-templates select="parameterdescription/para"/>
        </para>
      </listitem>
    </varlistentry>
</xsl:template>

<xsl:template match="parameterlist">
  <xsl:if test="parameteritem">
      <variablelist>
        <xsl:apply-templates select="parameteritem" />
      </variablelist>
  </xsl:if>
</xsl:template>

<xsl:template match="ref">
  <emphasis><xsl:apply-templates /></emphasis>
</xsl:template>

<xsl:template match="simplesect[@kind='return']">
  <variablelist>
    <varlistentry>
      <term>Returns:</term>
      <listitem>
        <para>
          <xsl:apply-templates />
        </para>
      </listitem>
    </varlistentry>
  </variablelist>
</xsl:template>

<xsl:template match="simplesect[@kind='see']">
  <itemizedlist>
    <listitem>
      <para>
        See also:
        <xsl:for-each select="para/ref">
          <emphasis><xsl:apply-templates /><xsl:text> </xsl:text></emphasis>
        </xsl:for-each>
      </para>
    </listitem>
  </itemizedlist>
</xsl:template>

<xsl:template match="simplesect[@kind='since']">
  <itemizedlist>
    <listitem>
      <para>
        Since: <xsl:apply-templates select="para"/>
      </para>
    </listitem>
  </itemizedlist>
</xsl:template>

<xsl:template match="simplesect[@kind='note']">
  <emphasis>Note: <xsl:apply-templates /></emphasis>
</xsl:template>

<xsl:template match="programlisting">
  <programlisting><xsl:apply-templates /></programlisting>
</xsl:template>

<!-- this opens a para for each detaileddescription/para. I could not find a
     way to extract the right text for the description from the
     source otherwise. Downside: we can't use para for return value, "see
     also", etc.  because they're already inside a para. So they're lists.

     It also means we don't control the order of when something is added to
     the output, it matches the input file
     -->
<xsl:template match="detaileddescription/para">
  <para><xsl:apply-templates /></para>
</xsl:template>

<xsl:template match="detaileddescription">
  <xsl:apply-templates select="para" />
</xsl:template>

<!-- methods -->
<xsl:template match="memberdef" >
  <xsl:if test="@kind = 'function' and @static = 'no'">
    <varlistentry>
        <term>
          <xsl:apply-templates select="name"/>
        - <xsl:apply-templates select="briefdescription" />
        </term>
        <listitem>
          <para>
            <synopsis>
              <xsl:apply-templates select="definition"/><xsl:apply-templates select="argsstring"/>
            </synopsis>
          </para>
          <xsl:apply-templates select="detaileddescription" />
        </listitem>
    </varlistentry>
    </xsl:if>
</xsl:template>

<!-- classes -->
<xsl:template match="compounddef" >
    <xsl:if test="@kind = 'class' ">
    <varlistentry>
        <term>
            <xsl:apply-templates select="compoundname" />
            <xsl:if test="briefdescription">
                - <xsl:apply-templates select="briefdescription" />
            </xsl:if>
        </term>

        <listitem>
          <xsl:apply-templates select="detaileddescription/para" />
        </listitem>
    </varlistentry>
    </xsl:if>
</xsl:template>
</xsl:stylesheet>
