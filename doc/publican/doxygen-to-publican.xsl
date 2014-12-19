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

  <appendix id="sect-Library-$which">
    <xsl:attribute name="id">sect-Library-<xsl:value-of select="$which"/></xsl:attribute>
    <title><xsl:value-of select="$which"/> API</title>

    <para>
      The open-source reference implementation of Wayland protocol is
      split in two C libraries, <link
      linkend="sect-Library-Client">libwayland-client</link> and <link
      linkend="sect-Library-Server">libwayland-server</link>. Their
      main responsibility is to handle the Inter-process communication
      (<emphasis>IPC</emphasis>) with each other, therefore
      guaranteeing the protocol objects marshaling and messages
      synchronization.
    </para>

    <para>
      Following is the Wayland library classes for the
      <emphasis>libwayland-<xsl:value-of select="translate($which,
      'SC', 'sc')"/></emphasis>.  This appendix describes in detail
      the library's methods and their helpers, aiming implementors who
      are building a Wayland <xsl:value-of select="translate($which,
      'SC', 'sc')"/>.
    </para>

    <xsl:apply-templates select="/doxygen/compounddef[@kind='class']" />

    <section id="{$which}-Functions">
      <title>Functions</title>
      <para />
      <variablelist>
        <xsl:apply-templates select="/doxygen/compounddef[@kind!='class']/sectiondef/memberdef" />
      </variablelist>
    </section>

  </appendix>
</xsl:template>

<xsl:template match="parameteritem">
    <varlistentry>
        <term>
          <xsl:apply-templates select="parameternamelist/parametername"/>
        </term>
      <listitem>
        <simpara><xsl:apply-templates select="parameterdescription"/></simpara>
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
  <link linkend="{$which}-{@refid}"><xsl:value-of select="." /></link>
</xsl:template>

<xsl:template match="simplesect[@kind='return']">
  <variablelist>
    <varlistentry>
      <term>Returns:</term>
      <listitem>
        <simpara><xsl:apply-templates /></simpara>
      </listitem>
    </varlistentry>
  </variablelist>
</xsl:template>

<xsl:template match="simplesect[@kind='see']">
  See also: <xsl:apply-templates />
</xsl:template>

<xsl:template match="simplesect[@kind='since']">
  Since: <xsl:apply-templates />
</xsl:template>

<xsl:template match="simplesect[@kind='note']">
  <emphasis>Note: <xsl:apply-templates /></emphasis>
</xsl:template>

<xsl:template match="sp">
  <xsl:text> </xsl:text>
</xsl:template>

<xsl:template match="programlisting">
  <programlisting><xsl:apply-templates /></programlisting>
</xsl:template>

<!-- stops cross-references in the section titles -->
<xsl:template match="briefdescription">
  <xsl:value-of select="." />
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
    <varlistentry id="{$which}-{@id}">
        <term>
          <xsl:value-of select="name"/>
          <xsl:if test="normalize-space(briefdescription) != ''">
            - <xsl:apply-templates select="briefdescription" />
          </xsl:if>
        </term>
        <listitem>
          <synopsis>
            <xsl:apply-templates select="definition"/><xsl:apply-templates select="argsstring"/>
          </synopsis>
          <xsl:apply-templates select="detaileddescription" />
        </listitem>
    </varlistentry>
  </xsl:if>
</xsl:template>

<!-- classes -->
<xsl:template match="compounddef" >
  <xsl:if test="@kind = 'class'">
    <section id="{$which}-{@id}">
        <title>
            <xsl:value-of select="compoundname" />
            <xsl:if test="normalize-space(briefdescription) != ''">
                - <xsl:apply-templates select="briefdescription" />
            </xsl:if>
        </title>

        <xsl:apply-templates select="detaileddescription" />

        <variablelist>
          <xsl:apply-templates select="sectiondef/memberdef" />
        </variablelist>
    </section>
  </xsl:if>
</xsl:template>
</xsl:stylesheet>
